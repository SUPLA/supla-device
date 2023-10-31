/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef EXTRAS_TEST_DOUBLES_SRPC_MOCK_H_
#define EXTRAS_TEST_DOUBLES_SRPC_MOCK_H_

#include <gmock/gmock.h>
#include <supla-common/proto.h>
#include <supla-common/srpc.h>

#include <vector>

class SrpcInterface {
 public:
  SrpcInterface();
  virtual ~SrpcInterface();

  virtual _supla_int_t valueChanged(void *srpc,
                                    unsigned char channelNumber,
                                    std::vector<char> value,
                                    unsigned char offline,
                                    unsigned _supla_int_t
                                        validity_time_sec) = 0;

  virtual _supla_int_t actionTrigger(unsigned char channel_number,
                                     int actionTrigger) = 0;

  virtual _supla_int_t srpc_dcs_async_set_activity_timeout(
      void *_srpc, TDCS_SuplaSetActivityTimeout *dcs_set_activity_timeout) = 0;
  virtual void srpc_params_init(TsrpcParams *params) = 0;
  virtual _supla_int_t srpc_ds_async_set_channel_result(
      void *_srpc,
      unsigned char ChannelNumber,
      _supla_int_t SenderID,
      char Success) = 0;
  virtual _supla_int_t srpc_ds_async_device_calcfg_result(
      void *_srpc, TDS_DeviceCalCfgResult *result) = 0;
  virtual void *srpc_init(TsrpcParams *params) = 0;
  virtual void srpc_free(void *srpc) = 0;
  virtual void srpc_rd_free(TsrpcReceivedData *rd) = 0;
  virtual char srpc_getdata(void *_srpc,
                            TsrpcReceivedData *rd,
                            unsigned _supla_int_t rr_id) = 0;
  virtual char srpc_iterate(void *_srpc) = 0;
  virtual void srpc_set_proto_version(void *_srpc, unsigned char version) = 0;
  virtual _supla_int_t srpc_ds_async_registerdevice_e(
      void *_srpc, TDS_SuplaRegisterDevice_E *registerdevice) = 0;
  virtual _supla_int_t srpc_dcs_async_ping_server(void *_srpc) = 0;
  virtual _supla_int_t srpc_csd_async_channel_state_result(
      void *_srpc, TDSC_ChannelState *state) = 0;
  virtual _supla_int_t srpc_dcs_async_get_user_localtime(void *_srpc) = 0;
  virtual _supla_int_t getChannelConfig(unsigned char channelNumber,
      unsigned char configType) = 0;

  virtual _supla_int_t setDeviceConfigResult(
      TSDS_SetDeviceConfigResult *result) = 0;
  virtual _supla_int_t setDeviceConfigRequest(
      TSDS_SetDeviceConfig *request) = 0;
  virtual _supla_int_t setChannelConfigResult(
      TSDS_SetChannelConfigResult *result) = 0;
  virtual _supla_int_t setChannelConfigRequest(
      TSDS_SetChannelConfig *request) = 0;

  virtual _supla_int_t registerPushNotification(
      int context, unsigned char ServerManagedFields) = 0;
  virtual _supla_int_t sendPushNotification(
      int context,
      unsigned char TitleSize,
      unsigned char BodySize,
      const signed char *TitleAndBody) = 0;

  static SrpcInterface *instance;
};

class SrpcMock : public SrpcInterface {
 public:
  SrpcMock();
  virtual ~SrpcMock();
  MOCK_METHOD(_supla_int_t,
              valueChanged,
              (void *,
               unsigned char,
               std::vector<char>,
               unsigned char,
               unsigned _supla_int_t),
              (override));
  MOCK_METHOD(_supla_int_t,
              srpc_dcs_async_set_activity_timeout,
              (void *, TDCS_SuplaSetActivityTimeout *),
              (override));
  MOCK_METHOD(void, srpc_params_init, (TsrpcParams *), (override));
  MOCK_METHOD(_supla_int_t,
              srpc_ds_async_set_channel_result,
              (void *, unsigned char, _supla_int_t, char),
              (override));
  MOCK_METHOD(_supla_int_t,
              srpc_ds_async_device_calcfg_result,
              (void *, TDS_DeviceCalCfgResult *),
              (override));
  MOCK_METHOD((void *), srpc_init, (TsrpcParams *), (override));
  MOCK_METHOD((void), srpc_free, (void *), (override));
  MOCK_METHOD(void, srpc_rd_free, (TsrpcReceivedData *), (override));
  MOCK_METHOD(char,
              srpc_getdata,
              (void *, TsrpcReceivedData *, unsigned _supla_int_t),
              (override));
  MOCK_METHOD(char, srpc_iterate, (void *), (override));
  MOCK_METHOD(void,
              srpc_set_proto_version,
              (void *, unsigned char),
              (override));
  MOCK_METHOD(_supla_int_t,
              srpc_ds_async_registerdevice_e,
              (void *, TDS_SuplaRegisterDevice_E *),
              (override));
  MOCK_METHOD(_supla_int_t, srpc_dcs_async_ping_server, (void *), (override));
  MOCK_METHOD(_supla_int_t,
              srpc_csd_async_channel_state_result,
              (void *, TDSC_ChannelState *),
              (override));
  MOCK_METHOD(_supla_int_t,
              srpc_dcs_async_get_user_localtime,
              (void *),
              (override));
  MOCK_METHOD(_supla_int_t,
              actionTrigger,
              (unsigned char channel_number, int actionTrigger),
              (override));
  MOCK_METHOD(_supla_int_t,
              getChannelConfig,
              (unsigned char channelNumber, unsigned char configType),
              (override));
  MOCK_METHOD(_supla_int_t, setDeviceConfigResult,
      (TSDS_SetDeviceConfigResult *result), (override));
  MOCK_METHOD(_supla_int_t, setDeviceConfigRequest,
      (TSDS_SetDeviceConfig *request), (override));
  MOCK_METHOD(_supla_int_t, setChannelConfigResult,
      (TSDS_SetChannelConfigResult *result), (override));
  MOCK_METHOD(_supla_int_t, setChannelConfigRequest,
      (TSDS_SetChannelConfig *request), (override));
  MOCK_METHOD(_supla_int_t,
              registerPushNotification,
              (int context, unsigned char ServerManagedFields),
              (override));
  MOCK_METHOD(_supla_int_t,
              sendPushNotification,
              (int context,
               unsigned char TitleSize,
               unsigned char BodySize,
               const signed char *TitleAndBody),
              (override));
};

#endif  // EXTRAS_TEST_DOUBLES_SRPC_MOCK_H_
