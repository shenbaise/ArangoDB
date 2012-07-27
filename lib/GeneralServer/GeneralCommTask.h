////////////////////////////////////////////////////////////////////////////////
/// @brief task for communications
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Achim Brandt
/// @author Copyright 2009-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_GENERAL_SERVER_GENERAL_COMM_TASK_H
#define TRIAGENS_GENERAL_SERVER_GENERAL_COMM_TASK_H 1

#include "Basics/Common.h"

#include "Basics/StringBuffer.h"
#include "Logger/Logger.h"
#include "Rest/HttpRequest.h"
#include "Scheduler/SocketTask.h"

// -----------------------------------------------------------------------------
// --SECTION--                                             class GeneralCommTask
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

namespace triagens {
  namespace rest {

////////////////////////////////////////////////////////////////////////////////
/// @brief task for general communication
////////////////////////////////////////////////////////////////////////////////

    template<typename S, typename HF>
    class GeneralCommTask : public SocketTask {
      private:
        GeneralCommTask (GeneralCommTask const&);
        GeneralCommTask const& operator= (GeneralCommTask const&);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructs a new task
////////////////////////////////////////////////////////////////////////////////

      public:

        GeneralCommTask (S* server, socket_t fd, ConnectionInfo const& info)
          : Task("GeneralCommTask"),
            SocketTask(fd),
            _server(server),
            _connectionInfo(info),
            _writeBuffers(),
#ifdef TRI_ENABLE_FIGURES
            _writeBuffersStats(),
#endif
            _readPosition(0),
            _bodyPosition(0),
            _bodyLength(0),
            _requestPending(false),
            _closeRequested(false),
            _request(0),
            _readRequestBody(false),
            _maximalHeaderSize(0),
            _maximalBodySize(0) {
          LOGGER_TRACE << "connection established, client " << fd
                       << ", server ip " << _connectionInfo.serverAddress
                       << ", server port " << _connectionInfo.serverPort
                       << ", client ip " <<  _connectionInfo.clientAddress
                       << ", client port " <<  _connectionInfo.clientPort;

          pair<size_t, size_t> p = server->getHandlerFactory()->sizeRestrictions();

          _maximalHeaderSize = p.first;
          _maximalBodySize = p.second;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief destructs a task
////////////////////////////////////////////////////////////////////////////////

      protected:

        ~GeneralCommTask () {
          LOGGER_TRACE << "connection closed, client " << commSocket;

          // free write buffers
          for (deque<basics::StringBuffer*>::iterator i = _writeBuffers.begin();  i != _writeBuffers.end();  i++) {
            basics::StringBuffer* buffer = *i;

            delete buffer;
          }

#ifdef TRI_ENABLE_FIGURES

          for (deque<TRI_request_statistics_t*>::iterator i = _writeBuffersStats.begin();  i != _writeBuffersStats.end();  i++) {
            TRI_request_statistics_t* buffer = *i;

            TRI_ReleaseRequestStatistics(buffer);
          }

#endif

          // free http request
          if (_request != 0) {
            delete _request;
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief handles response
////////////////////////////////////////////////////////////////////////////////

        void handleResponse (typename HF::GeneralResponse * response)  {
          _requestPending = false;

          addResponse(response);
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief closes the socket
////////////////////////////////////////////////////////////////////////////////

        void beginShutdown ()  {
          if (commSocket != -1) {
            close(commSocket);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                         virtual protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief reads data from the socket
////////////////////////////////////////////////////////////////////////////////

        virtual bool processRead () = 0;

////////////////////////////////////////////////////////////////////////////////
/// @brief reads data from the socket
////////////////////////////////////////////////////////////////////////////////

        virtual void addResponse (typename HF::GeneralResponse * response) = 0;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                 protected methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the write buffer
////////////////////////////////////////////////////////////////////////////////

        void fillWriteBuffer () {
          if (! hasWriteBuffer() && ! _writeBuffers.empty()) {
            basics::StringBuffer * buffer = _writeBuffers.front();
            _writeBuffers.pop_front();

#ifdef TRI_ENABLE_FIGURES
            TRI_request_statistics_t* statistics = _writeBuffersStats.front();
            _writeBuffersStats.pop_front();
#else
            TRI_request_statistics_t* statistics = 0;
#endif

            setWriteBuffer(buffer, statistics);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                SocketTask methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool handleRead (bool& closed)  {
          bool res = fillReadBuffer(closed);

          if (res) {
            if (_request == 0 || _readRequestBody) {
              res = processRead();
            }
          }

          if (closed) {
            res = false;
            _server->handleCommunicationClosed(this);
          }
          else if (! res) {
            _server->handleCommunicationFailure(this);
          }

          return res;
        }

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void completedWriteBuffer (bool& closed) {
          _writeBuffer = 0;

#ifdef TRI_ENABLE_FIGURES
          if (_writeBufferStatistics != 0) {
            _writeBufferStatistics->_writeEnd = TRI_StatisticsTime();

            TRI_ReleaseRequestStatistics(_writeBufferStatistics);
            _writeBufferStatistics = 0;
          }
#endif

          fillWriteBuffer();

          if (_closeRequested && ! hasWriteBuffer() && _writeBuffers.empty()) {
            closed = true;
            _server->handleCommunicationClosed(this);
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                               protected variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup GeneralServer
/// @{
////////////////////////////////////////////////////////////////////////////////

      protected:

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying server
////////////////////////////////////////////////////////////////////////////////

        S * const _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief connection info
////////////////////////////////////////////////////////////////////////////////

        ConnectionInfo const _connectionInfo;

////////////////////////////////////////////////////////////////////////////////
/// @brief write buffers
////////////////////////////////////////////////////////////////////////////////

        deque<basics::StringBuffer*> _writeBuffers;

////////////////////////////////////////////////////////////////////////////////
/// @brief statistics buffers
////////////////////////////////////////////////////////////////////////////////

#ifdef TRI_ENABLE_FIGURES

        deque<TRI_request_statistics_t*> _writeBuffersStats;

#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief current read position
////////////////////////////////////////////////////////////////////////////////

        size_t _readPosition;

////////////////////////////////////////////////////////////////////////////////
/// @brief start of the body position
////////////////////////////////////////////////////////////////////////////////

        size_t _bodyPosition;

////////////////////////////////////////////////////////////////////////////////
/// @brief body length
////////////////////////////////////////////////////////////////////////////////

        size_t _bodyLength;

////////////////////////////////////////////////////////////////////////////////
/// @brief true if request is complete but not handled
////////////////////////////////////////////////////////////////////////////////

        bool _requestPending;

////////////////////////////////////////////////////////////////////////////////
/// @brief true if a close has been requested by the client
////////////////////////////////////////////////////////////////////////////////

        bool _closeRequested;

////////////////////////////////////////////////////////////////////////////////
/// @brief the request with possible incomplete body
////////////////////////////////////////////////////////////////////////////////

        typename HF::GeneralRequest* _request;

////////////////////////////////////////////////////////////////////////////////
/// @brief true if reading the request body
////////////////////////////////////////////////////////////////////////////////

        bool _readRequestBody;

////////////////////////////////////////////////////////////////////////////////
/// @brief the maximal header size
////////////////////////////////////////////////////////////////////////////////

        size_t _maximalHeaderSize;

////////////////////////////////////////////////////////////////////////////////
/// @brief the maximal body size
////////////////////////////////////////////////////////////////////////////////

        size_t _maximalBodySize;
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
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}\\)"
// End:
