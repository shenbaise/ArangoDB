////////////////////////////////////////////////////////////////////////////////
/// @brief application http server feature
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
/// @author Copyright 2010-2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationHttpServer.h"

#include "Basics/delete_object.h"
#include "Dispatcher/ApplicationDispatcher.h"
#include "HttpServer/HttpHandler.h"
#include "HttpServer/HttpServer.h"
#include "Logger/Logger.h"
#include "Scheduler/ApplicationScheduler.h"

using namespace triagens::basics;
using namespace triagens::rest;
using namespace std;

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

ApplicationHttpServer::ApplicationHttpServer (ApplicationServer* applicationServer,
                                              ApplicationScheduler* applicationScheduler,
                                              ApplicationDispatcher* applicationDispatcher,
                                              std::string const& authenticationRealm,
                                              HttpHandlerFactory::auth_fptr checkAuthentication)
  : ApplicationFeature("HttpServer"),
    _applicationServer(applicationServer),
    _applicationScheduler(applicationScheduler),
    _applicationDispatcher(applicationDispatcher),
    _handlerFactory(0), 
    _authenticationRealm(authenticationRealm),
    _checkAuthentication(checkAuthentication),
    _servers() {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

ApplicationHttpServer::~ApplicationHttpServer () {
  for_each(_servers.begin(), _servers.end(), DeleteObject());
  _servers.clear();

  if (_handlerFactory != 0) {
    delete _handlerFactory;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Scheduler
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief builds the http server
////////////////////////////////////////////////////////////////////////////////

HttpServer* ApplicationHttpServer::buildServer (const EndpointList* endpointList) {
  assert(_handlerFactory != 0);
  assert(_applicationScheduler->scheduler() != 0);
   
  HttpServer* server = new HttpServer(_applicationScheduler->scheduler(), 
                                      _applicationDispatcher->dispatcher(), 
                                      _handlerFactory); 

  // keep a list of active servers
  _servers.push_back(server);

  // open http ports
  server->setEndpointList(endpointList);

  return server;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup ApplicationServer
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpServer::setupOptions (map<string, ProgramOptionsDescription>& options) {
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationHttpServer::parsePhase2 (ProgramOptions& options) {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationHttpServer::prepare2 () {
  _handlerFactory = new HttpHandlerFactory(_authenticationRealm, _checkAuthentication);

  Scheduler* scheduler = _applicationScheduler->scheduler();
 
  if (scheduler == 0) {
    LOGGER_FATAL << "no scheduler is known, cannot create http server";

    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

bool ApplicationHttpServer::open () {
  for (vector<HttpServer*>::iterator i = _servers.begin();  i != _servers.end();  ++i) {
    HttpServer* server = *i;

    server->startListening();
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpServer::close () {

  // close all open connections
  for (vector<HttpServer*>::iterator i = _servers.begin();  i != _servers.end();  ++i) {
    HttpServer* server = *i;

    server->shutdownHandlers();
  }

  // close all listen sockets
  for (vector<HttpServer*>::iterator i = _servers.begin();  i != _servers.end();  ++i) {
    HttpServer* server = *i;

    server->stopListening();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

void ApplicationHttpServer::stop () {
  for (vector<HttpServer*>::iterator i = _servers.begin();  i != _servers.end();  ++i) {
    HttpServer* server = *i;

    server->stop();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|// --SECTION--\\|/// @\\}\\)"
// End:
