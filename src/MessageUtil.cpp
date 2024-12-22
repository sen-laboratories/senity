/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "MessageUtil.h"

// LATER: we could also parse a fmt like string like "name=%s, number=%d"
BMessage* MessageUtil::CreateBMessage(BMessage* msg, const char* k, const char* v) {
    msg->AddString(k, v);
    return msg;
}

BMessage* MessageUtil::CreateBMessage(BMessage* msg, const char* k, const char* v, const char* k2, const char* v2) {
    BMessage *msg2 = CreateBMessage(msg, k, v);
    msg->Append(*msg2);

    msg2 = CreateBMessage(msg, k2, v2);
    msg->Append(*msg2);

    return msg;
}