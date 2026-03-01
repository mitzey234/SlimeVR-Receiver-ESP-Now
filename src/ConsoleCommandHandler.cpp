#include "ConsoleCommandHandler.h"
#include "commands/CommandDispatcher.h"

void ConsoleCommandHandler::update() {
    static String serialBuffer;
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            serialBuffer.trim();
            if (serialBuffer.length() > 0) {
                processConsoleCommand(serialBuffer);
            }
            serialBuffer = "";
        } else {
            serialBuffer += c;
        }
    }
}
