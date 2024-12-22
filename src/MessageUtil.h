/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <Message.h>

class MessageUtil {
    public:
    /**
     * convenience constructor for simple key/value messages which contain only Strings
     */
    static BMessage* CreateBMessage(BMessage* msg, const char* k, const char* v);
    static BMessage* CreateBMessage(BMessage* msg, const char* k, const char* v, const char* k2, const char* v2);
};
