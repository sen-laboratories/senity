/**
 * @author Gregor Rosenauer <gregor.rosenauer@gmail.com>
 * All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */

#pragma once

#include <Entry.h>
#include <sys/types.h>

 class SenConnector {
    public:
        static status_t QueryForSenId(entry_ref *ref, const char *senId);
 };
