// Copyright 2015 The Chromium Authors. All rights reserved.             
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.                                            
                                                                         
#include "device/serial/serial_device_enumerator.h"              
                                                                         
namespace device {                                                       
                                                                         
// static                                                                
std::unique_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create() {    
  return NULL;
}                                                                        
                                                                         
std::vector<serial::DeviceInfoPtr> SerialDeviceEnumerator::GetDevices() {
  NOTIMPLEMENTED();                                                      
  std::vector<serial::DeviceInfoPtr> devices;
  return devices;                    
}                                                                        
                                                                         
}  // namespace device                                                   