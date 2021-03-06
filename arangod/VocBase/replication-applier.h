////////////////////////////////////////////////////////////////////////////////
/// @brief replication applier
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
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_VOC_BASE_REPLICATION_APPLIER_H
#define TRIAGENS_VOC_BASE_REPLICATION_APPLIER_H 1

#include "BasicsC/common.h"
#include "BasicsC/locks.h"
#include "BasicsC/threads.h"
#include "Replication/replication-static.h"
#include "VocBase/replication-common.h"
#include "VocBase/voc-types.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_json_s;
struct TRI_transaction_s;
struct TRI_vocbase_s;

// -----------------------------------------------------------------------------
// --SECTION--                                               REPLICATION APPLIER
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief struct containing a replication apply configuration
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_applier_configuration_s {
  char*         _endpoint;
  char*         _database;
  char*         _username;
  char*         _password;
  double        _requestTimeout;
  double        _connectTimeout;
  uint64_t      _ignoreErrors;
  uint64_t      _maxConnectRetries;
  uint64_t      _chunkSize;
  uint32_t      _sslProtocol;
  bool          _autoStart;
  bool          _adaptivePolling;
}
TRI_replication_applier_configuration_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief struct containing a replication apply error
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_applier_error_s {
  int           _code;
  char*         _msg;
  char          _time[24];
}
TRI_replication_applier_error_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief state information about replication application
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_applier_state_s {
  TRI_voc_tick_t                           _lastProcessedContinuousTick;
  TRI_voc_tick_t                           _lastAppliedContinuousTick;
  TRI_voc_tick_t                           _lastAvailableContinuousTick;
  bool                                     _active;
  char*                                    _progressMsg;
  char                                     _progressTime[24];
  TRI_server_id_t                          _serverId;
  TRI_replication_applier_error_t          _lastError;
  uint64_t                                 _failedConnects;
  uint64_t                                 _totalRequests;
  uint64_t                                 _totalFailedConnects;
  uint64_t                                 _totalEvents;
}
TRI_replication_applier_state_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief replication applier
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_replication_applier_s {
  struct TRI_vocbase_s*                    _vocbase;
  TRI_read_write_lock_t                    _statusLock;
  TRI_spin_t                               _threadLock;
  TRI_condition_t                          _runStateChangeCondition;
  bool                                     _terminateThread;
  TRI_replication_applier_state_t          _state;
  TRI_replication_applier_configuration_t  _configuration;
  char*                                    _databaseName;
  TRI_thread_t                             _thread;
}
TRI_replication_applier_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create a replication applier
////////////////////////////////////////////////////////////////////////////////

TRI_replication_applier_t* TRI_CreateReplicationApplier (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a replication applier
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyReplicationApplier (TRI_replication_applier_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief free a replication applier
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeReplicationApplier (TRI_replication_applier_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Replication
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief checks whether the apply thread should terminate
////////////////////////////////////////////////////////////////////////////////

bool TRI_WaitReplicationApplier (TRI_replication_applier_t*, 
                                 uint64_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a JSON representation of the replication apply configuration
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_s* TRI_JsonConfigurationReplicationApplier (TRI_replication_applier_configuration_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief start the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_StartReplicationApplier (TRI_replication_applier_t*,
                                 TRI_voc_tick_t,
                                 bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_StopReplicationApplier (TRI_replication_applier_t*,
                                bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief configure the replication applier
////////////////////////////////////////////////////////////////////////////////

int TRI_ConfigureReplicationApplier (TRI_replication_applier_t*,
                                     TRI_replication_applier_configuration_t const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get the current replication apply state
////////////////////////////////////////////////////////////////////////////////

int TRI_StateReplicationApplier (TRI_replication_applier_t*,
                                 TRI_replication_applier_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief get a JSON representation of an applier
////////////////////////////////////////////////////////////////////////////////

struct TRI_json_s* TRI_JsonReplicationApplier (TRI_replication_applier_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief register an applier error
////////////////////////////////////////////////////////////////////////////////

int TRI_SetErrorReplicationApplier (TRI_replication_applier_t*,
                                    int,
                                    char const*);

////////////////////////////////////////////////////////////////////////////////
/// @brief set the progress with or without a lock
////////////////////////////////////////////////////////////////////////////////

void TRI_SetProgressReplicationApplier (TRI_replication_applier_t*,
                                        char const*,
                                        bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise an apply state struct
////////////////////////////////////////////////////////////////////////////////

void TRI_InitStateReplicationApplier (TRI_replication_applier_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an apply state struct
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyStateReplicationApplier (TRI_replication_applier_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief save the replication application state to a file
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveStateReplicationApplier (struct TRI_vocbase_s*,
                                     TRI_replication_applier_state_t const*,
                                     bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the replication application state file
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveStateReplicationApplier (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief load the replication application state from a file
////////////////////////////////////////////////////////////////////////////////

int TRI_LoadStateReplicationApplier (struct TRI_vocbase_s*,
                                     TRI_replication_applier_state_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise an apply configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_InitConfigurationReplicationApplier (TRI_replication_applier_configuration_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy an apply configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyConfigurationReplicationApplier (TRI_replication_applier_configuration_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief copy an apply configuration
////////////////////////////////////////////////////////////////////////////////

void TRI_CopyConfigurationReplicationApplier (TRI_replication_applier_configuration_t const*,
                                              TRI_replication_applier_configuration_t*);

////////////////////////////////////////////////////////////////////////////////
/// @brief remove the replication application configuration file
////////////////////////////////////////////////////////////////////////////////

int TRI_RemoveConfigurationReplicationApplier (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief save the replication application configuration to a file
////////////////////////////////////////////////////////////////////////////////

int TRI_SaveConfigurationReplicationApplier (struct TRI_vocbase_s*,
                                             TRI_replication_applier_configuration_t const*,
                                             bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief stop the applier and "forget" everything
////////////////////////////////////////////////////////////////////////////////
  
int TRI_ForgetReplicationApplier (TRI_replication_applier_t*);

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
}
#endif

#endif

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
