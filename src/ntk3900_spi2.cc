#include <node.h>
#include <napi.h>

#include "spi_driver.h"

// Entry point for the module

using namespace Napi;

Napi::Object Init(Napi::Env env, Napi::Object exports) {

  SPIDriver::Init(env, exports);

  return exports;
}



NODE_API_MODULE(NODE_GYP_MODULE_NAME, Init)
