/*
 * Arduino IDE entry point.
 *
 * 実装本体はsrc/main.cppのappSetup()/appLoop()にあります。
 * PlatformIOではこのファイルはビルド対象外となり、src/main.cppが
 * setup()/loop()を提供します。
 */

#include "src/app.h"

void setup() {
  appSetup();
}

void loop() {
  appLoop();
}
