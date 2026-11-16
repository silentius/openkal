/*
 * OpenKal - an open source KalOnline client and server.
 * Copyright (C) 2026 OpenKal contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

#include <cstdint>

namespace openkal::protocol {

    enum class CloseConnect : std::uint8_t {
        Normal = 0,
        Kick,
        SamePlayer,
        SameUser,
        Hack,
        Expired,
        Overpopulation,
        Error,
        ReloginForCopyPlayer,
        RestorePlayer,
        WrongProtocolVersion,
        DetectedHack,
        Checking,
        OverPending,
        HackShieldCrc,          // not used by openkal ? never implement HackShield
        HackShieldClientError,  // not used by openkal
        SkillInitializationFailed,
    };

    enum class Answer : std::uint8_t {
        Error = 0,
        Ok,
        MlmOk,
        ErrorDissolution,
    };

    enum class LoginAns : std::uint8_t {
        Error = 0,
        Ok,
        WrongId,
        WrongPwd,
        SameUser,
        Blocked,
        Expired,
        AgeLimited,
        NotPermitted,
        LoginLater,
        AgeLimited2,
        NewAccountConnectLimit,
        SecondPwdReq,
        WcWrongId,
        SecondPwdChangeReq,
    };

}  // namespace openkal::protocol
