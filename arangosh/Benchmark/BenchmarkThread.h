////////////////////////////////////////////////////////////////////////////////
/// @brief benchmark thread
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_BENCHMARK_BENCHMARK_THREAD_H
#define TRIAGENS_BENCHMARK_BENCHMARK_THREAD_H 1

#include "Basics/Common.h"

#include "BasicsC/hashes.h"

#include "Basics/ConditionLocker.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Thread.h"
#include "BasicsC/logging.h"
#include "Rest/HttpResponse.h"
#include "SimpleHttpClient/SimpleHttpClient.h"
#include "SimpleHttpClient/GeneralClientConnection.h"
#include "Benchmark/BenchmarkCounter.h"
#include "Benchmark/BenchmarkOperation.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::httpclient;
using namespace triagens::rest;

namespace triagens {
  namespace arangob {

// -----------------------------------------------------------------------------
// --SECTION--                                             class BenchmarkThread
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

    class BenchmarkThread : public Thread {

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief construct the benchmark thread
////////////////////////////////////////////////////////////////////////////////

        BenchmarkThread (BenchmarkOperation* operation,
                         ConditionVariable* condition,
                         void (*callback) (),
                         int threadNumber,
                         const unsigned long batchSize,
                         BenchmarkCounter<unsigned long>* operationsCounter,
                         Endpoint* endpoint,
                         const string& databaseName,
                         const string& username,
                         const string& password,
                         double requestTimeout,
                         double connectTimeout,
                         uint32_t sslProtocol,
                         bool keepAlive,
                         bool async)
          : Thread("arangob"),
            _operation(operation),
            _startCondition(condition),
            _callback(callback),
            _threadNumber(threadNumber),
            _batchSize(batchSize),
            _warningCount(0),
            _operationsCounter(operationsCounter),
            _endpoint(endpoint),
            _headers(),
            _databaseName(databaseName),
            _username(username),
            _password(password),
            _requestTimeout(requestTimeout),
            _connectTimeout(connectTimeout),
            _sslProtocol(sslProtocol),
            _keepAlive(keepAlive),
            _async(async),
            _client(0),
            _connection(0),
            _offset(0),
            _counter(0),
            _time(0.0) {

          _errorHeader = StringUtils::tolower(HttpResponse::getBatchErrorHeader());
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the benchmark thread
////////////////////////////////////////////////////////////////////////////////

        ~BenchmarkThread () {
          if (_client != 0) {
            delete _client;
          }

          if (_connection != 0) {
            delete _connection;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         virtual protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Threading
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the thread program
////////////////////////////////////////////////////////////////////////////////

        virtual void run () {
          _connection = GeneralClientConnection::factory(_endpoint, _requestTimeout, _connectTimeout, 3, _sslProtocol);

          if (_connection == 0) {
            LOG_FATAL_AND_EXIT("out of memory");
          }
         
          _client = new SimpleHttpClient(_connection, 10.0, true);

          if (_client == 0) {
            LOG_FATAL_AND_EXIT("out of memory");
          }
  
          _client->setLocationRewriter(this, &rewriteLocation);
          
          _client->setUserNamePassword("/", _username, _password);
          _client->setKeepAlive(_keepAlive);

          // test the connection
          SimpleHttpResult* result = _client->request(HttpRequest::HTTP_REQUEST_GET,
                                                      "/_api/version",
                                                      0,
                                                      0,
                                                      _headers);

          if (! result || ! result->isComplete()) {
            if (result) {
              delete result;
            }

            LOG_FATAL_AND_EXIT("could not connect to server");
          }

          delete result;

          // if we're the first thread, set up the test
          if (_threadNumber == 0) {
            if (! _operation->setUp(_client)) {
              LOG_FATAL_AND_EXIT("could not set up the test");
            }
          }

          if (_async) {
            _headers["x-arango-async"] = "true";
          }

          _callback();

          // wait for start condition to be broadcasted
          {
            ConditionLocker guard(_startCondition);
            guard.wait();
          }

          while (1) {
            unsigned long numOps = _operationsCounter->next(_batchSize);

            if (numOps == 0) {
              break;
            }

            if (_batchSize < 1) {
              executeSingleRequest();
            }
            else {
              executeBatchRequest(numOps);
            }
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief request location rewriter (injects database name)
////////////////////////////////////////////////////////////////////////////////

        static string rewriteLocation (void* data, const string& location) {
          BenchmarkThread* t = static_cast<BenchmarkThread*>(data);

          assert(t != 0);

          if (location.substr(0, 5) == "/_db/") {
            // location already contains /_db/
            return location;
          }

          if (location[0] == '/') {
            return string("/_db/" + t->_databaseName + location);
          }
          else {
            return string("/_db/" + t->_databaseName + "/" + location);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a batch request with numOperations parts
////////////////////////////////////////////////////////////////////////////////

        void executeBatchRequest (const unsigned long numOperations) {
          static const string boundary = "XXXarangob-benchmarkXXX";

          StringBuffer batchPayload(TRI_UNKNOWN_MEM_ZONE);

          for (unsigned long i = 0; i < numOperations; ++i) {
            // append boundary
            batchPayload.appendText("--" + boundary + "\r\n");
            // append content-type, this will also begin the body
            batchPayload.appendText("Content-Type: ", 14);
            batchPayload.appendText(HttpRequest::getPartContentType());
            batchPayload.appendText("\r\n\r\n", 4);

            // everything else (i.e. part request header & body) will get into the body
            const size_t threadCounter = _counter++;
            const size_t globalCounter = _offset + threadCounter;
            const string url = _operation->url(_threadNumber, threadCounter, globalCounter);
            size_t payloadLength = 0;
            bool mustFree = false;
            const char* payload = _operation->payload(&payloadLength, _threadNumber, threadCounter, globalCounter, &mustFree);
            const HttpRequest::HttpRequestType type = _operation->type(_threadNumber, threadCounter, globalCounter);

            // headline, e.g. POST /... HTTP/1.1
            HttpRequest::appendMethod(type, &batchPayload);
            batchPayload.appendText(url + " HTTP/1.1\r\n");
            batchPayload.appendText("\r\n", 2);

            // body
            batchPayload.appendText(payload, payloadLength);
            batchPayload.appendText("\r\n", 2);

            if (mustFree) {
              TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*) payload);
            }
          }

          // end of MIME
          batchPayload.appendText("--" + boundary + "--\r\n");

          _headers.erase("Content-Type");
          _headers["Content-Type"] = HttpRequest::getMultipartContentType() +
                                     "; boundary=" + boundary;

          double start = TRI_microtime();
          SimpleHttpResult* result = _client->request(HttpRequest::HTTP_REQUEST_POST,
                                                      "/_api/batch",
                                                      batchPayload.c_str(),
                                                      batchPayload.length(),
                                                      _headers);
          _time += TRI_microtime() - start;

          if (result == 0 || ! result->isComplete()) {
            _operationsCounter->incFailures(numOperations);
            if (result != 0) {
              delete result;
            }
            _warningCount++;
            if (_warningCount < MaxWarnings) {
              LOG_WARNING("batch operation failed because server did not reply");
            }
            return;
          }

          if (result->wasHttpError()) {
            _operationsCounter->incFailures(numOperations);

            _warningCount++;
            if (_warningCount < MaxWarnings) {
              LOG_WARNING("batch operation failed with HTTP code %d", (int) result->getHttpReturnCode());
            }
            else if (_warningCount == MaxWarnings) {
              LOG_WARNING("...more warnings...");
            }
          }
          else {
            const std::map<string, string>& headers = result->getHeaderFields();
            map<string, string>::const_iterator it = headers.find(_errorHeader);

            if (it != headers.end()) {
              size_t errorCount = (size_t) StringUtils::uint32((*it).second);

              if (errorCount > 0) {
                _operationsCounter->incFailures(errorCount);
              }
            }
          }
          delete result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a single request
////////////////////////////////////////////////////////////////////////////////

        void executeSingleRequest () {
          const size_t threadCounter = _counter++;
          const size_t globalCounter = _offset + threadCounter;
          const HttpRequest::HttpRequestType type = _operation->type(_threadNumber, threadCounter, globalCounter);
          const string url = _operation->url(_threadNumber, threadCounter, globalCounter);
          size_t payloadLength = 0;
          bool mustFree = false;

          // std::cout << "thread number #" << _threadNumber << ", threadCounter " << threadCounter << ", globalCounter " << globalCounter << "\n";
          const char* payload = _operation->payload(&payloadLength, _threadNumber, threadCounter, globalCounter, &mustFree);

          double start = TRI_microtime();
          SimpleHttpResult* result = _client->request(type,
                                                      url,
                                                      payload,
                                                      payloadLength,
                                                      _headers);
          _time += TRI_microtime() - start;

          if (mustFree) {
            TRI_Free(TRI_UNKNOWN_MEM_ZONE, (void*) payload);
          }

          if (result == 0 || ! result->isComplete()) {
            _operationsCounter->incFailures(1);
            if (result != 0) {
              delete result;
            }
            _warningCount++;
            if (_warningCount < MaxWarnings) {
              LOG_WARNING("batch operation failed because server did not reply");
            }
            return;
          }

          if (result->wasHttpError()) {
            _operationsCounter->incFailures(1);

            _warningCount++;
            if (_warningCount < MaxWarnings) {
              LOG_WARNING("request for URL '%s' failed with HTTP code %d", url.c_str(), (int) result->getHttpReturnCode());
            }
            else if (_warningCount == MaxWarnings) {
              LOG_WARNING("...more warnings...");
            }
          }
          delete result;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief set the threads offset value
////////////////////////////////////////////////////////////////////////////////

        void setOffset (size_t offset) {
          _offset = offset;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the total time accumulated by the thread
////////////////////////////////////////////////////////////////////////////////

        double getTime () const {
          return _time;
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Benchmark
/// @{
////////////////////////////////////////////////////////////////////////////////

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the operation to benchmark
////////////////////////////////////////////////////////////////////////////////

        BenchmarkOperation* _operation;

////////////////////////////////////////////////////////////////////////////////
/// @brief condition variable
////////////////////////////////////////////////////////////////////////////////

        ConditionVariable* _startCondition;

////////////////////////////////////////////////////////////////////////////////
/// @brief start callback function
////////////////////////////////////////////////////////////////////////////////

        void (*_callback) ();

////////////////////////////////////////////////////////////////////////////////
/// @brief our thread number
////////////////////////////////////////////////////////////////////////////////

        int _threadNumber;

////////////////////////////////////////////////////////////////////////////////
/// @brief batch size
////////////////////////////////////////////////////////////////////////////////

        const unsigned long _batchSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief warning counter
////////////////////////////////////////////////////////////////////////////////

        int _warningCount;

////////////////////////////////////////////////////////////////////////////////
/// @brief benchmark counter
////////////////////////////////////////////////////////////////////////////////

        BenchmarkCounter<unsigned long>* _operationsCounter;

////////////////////////////////////////////////////////////////////////////////
/// @brief endpoint to use
////////////////////////////////////////////////////////////////////////////////

        Endpoint* _endpoint;

////////////////////////////////////////////////////////////////////////////////
/// @brief extra request headers
////////////////////////////////////////////////////////////////////////////////

        map<string, string> _headers;

////////////////////////////////////////////////////////////////////////////////
/// @brief database name
////////////////////////////////////////////////////////////////////////////////

        const string _databaseName;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP username
////////////////////////////////////////////////////////////////////////////////

        const string _username;

////////////////////////////////////////////////////////////////////////////////
/// @brief HTTP password
////////////////////////////////////////////////////////////////////////////////

        const string _password;

////////////////////////////////////////////////////////////////////////////////
/// @brief the request timeout (in s)
////////////////////////////////////////////////////////////////////////////////

        double _requestTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief the connection timeout (in s)
////////////////////////////////////////////////////////////////////////////////

        double _connectTimeout;

////////////////////////////////////////////////////////////////////////////////
/// @brief ssl protocol
////////////////////////////////////////////////////////////////////////////////

        uint32_t _sslProtocol;

////////////////////////////////////////////////////////////////////////////////
/// @brief use HTTP keep-alive
////////////////////////////////////////////////////////////////////////////////

        bool _keepAlive;

////////////////////////////////////////////////////////////////////////////////
/// @brief send async requests
////////////////////////////////////////////////////////////////////////////////

        bool _async;

////////////////////////////////////////////////////////////////////////////////
/// @brief underlying client
////////////////////////////////////////////////////////////////////////////////

        triagens::httpclient::SimpleHttpClient* _client;

////////////////////////////////////////////////////////////////////////////////
/// @brief connection to the server
////////////////////////////////////////////////////////////////////////////////

        triagens::httpclient::GeneralClientConnection* _connection;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread offset value
////////////////////////////////////////////////////////////////////////////////

        size_t _offset;

////////////////////////////////////////////////////////////////////////////////
/// @brief thread counter value
////////////////////////////////////////////////////////////////////////////////

        size_t _counter;

////////////////////////////////////////////////////////////////////////////////
/// @brief time
////////////////////////////////////////////////////////////////////////////////

        double _time;

////////////////////////////////////////////////////////////////////////////////
/// @brief lower-case error header we look for
////////////////////////////////////////////////////////////////////////////////

        string _errorHeader;

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum number of warnings to be displayed per thread
////////////////////////////////////////////////////////////////////////////////

        static const int MaxWarnings = 5;

    };
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
