/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: BSD-3-Clause-Clear
 */

#pragma once
#include "ISession.h"

using namespace ::com::quic::sensinghub::session::V1_0;

class qsh_register_cb {
public:
  qsh_register_cb(ISession::respCallBack resp_cb,
                  ISession::errorCallBack error_cb,
                  ISession::eventCallBack event_cb
                  ) {
    _resp_cb = resp_cb;
    _error_cb = error_cb;
    _event_cb = event_cb;
  }

  inline ISession::respCallBack get_resp_cb() { return _resp_cb;}
  inline ISession::errorCallBack get_error_cb() { return _error_cb;}
  inline ISession::eventCallBack get_event_cb() { return _event_cb;}

private:
  ISession::respCallBack _resp_cb;
  ISession::errorCallBack _error_cb;
  ISession::eventCallBack _event_cb;
};
