// Copyright 2015 The Chromium Authors. All rights reserved.             
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.                                            
                                                                         
#include "device/serial/serial_device_enumerator.h"              
                                                                         
namespace device {                                                       
                                                                         
// static                                                                
std::unique_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create() {    
  return NULL;
}                                                                        
                                                                         
mojo::Array<serial::DeviceInfoPtr> SerialDeviceEnumerator::GetDevices() {
  NOTIMPLEMENTED();                                                      
  mojo::Array<serial::DeviceInfoPtr> devices;
  return devices;                    
}                                                                        
                                                                         
}  // namespace device                                                   