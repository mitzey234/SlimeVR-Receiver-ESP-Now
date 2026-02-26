#include "CommandDispatcher.h"

#include "FactoryResetCommand.h"
#include "GetChannelCommand.h"
#include "PairCommand.h"
#include "RebootCommand.h"
#include "ScanEnvCommand.h"
#include "SetChannelCommand.h"
#include "SetSecurityCommand.h"
#include "StartOtaUpdateCommand.h"
#include "SCOFFCommand.h"
#include "TemperatureCommand.h"
#include "UnknownCommand.h"
#include "UnpairAllCommand.h"
#include "UnpairCommand.h"
#include "SCInitCommand.h"

void processConsoleCommand(const String &command) {
    if (handleFactoryResetCommand(command)) return;
    if (handlePairCommand(command)) return;
    if (handleSetSecurityCommand(command)) return;
    if (handleSetChannelCommand(command)) return;
    if (handleUnpairCommand(command)) return;
    if (handleStartOtaUpdateCommand(command)) return;
    if (handleRebootCommand(command)) return;
    if (handleGetChannelCommand(command)) return;
    if (handleScanEnvCommand(command)) return;
    if (handleSCOFFCommand(command)) return;
    if (handleTemperatureCommand(command)) return;
    if (handleUnpairAllCommand(command)) return;
    if (handleSCInitCommand(command)) return;

    handleUnknownCommand();
}