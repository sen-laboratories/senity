/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <SupportDefs.h>

/*
 * core editor functionality
 */

// settings
static const uint32 MSG_SETTINGS          = 'Conf';
static const uint32 MSG_SETTINGS_CHANGED  = 'Cfgc';

// file handling
static const uint32 MSG_FILE_NEW  = 'Fnew';
static const uint32 MSG_FILE_OPEN = 'Fopn';
static const uint32 MSG_FILE_SAVE = 'Fsav';

// editing
// strange there is no API definition for this, only a hook method...
static const uint32 MSG_SELECTION_CHANGED = 'SLch';
// ditto
static const uint32 MSG_WINDOW_CLOSED     = 'Wcls';

/*
 * panels
 */
// event
static const uint32 MSG_PANEL_SHOW        = 'Psho';
// config
static const uint32 MSG_PANEL_CONF        = 'Pcfg';

// plugin config/message keys
#define CONF_PLUGIN_ID                      "plug:id"
#define CONF_PLUGIN_NAME                    "plug:name"
#define CONF_PLUGIN_TYPE                    "plug:type"
#define CONF_PLUGIN_ACTIVE                  "plug:active"
#define CONF_PLUGIN_SHOW                    "plug:show"

enum plugin_type {
    PLUGIN_TYPE_IMPORT,
    PLUGIN_TYPE_EXPORT,
    PLUGIN_TYPE_PANEL,
    PLUGIN_TYPE_RENDER,
    PLUGIN_TYPE_VALIDATE
};

// outline panel
static const uint32 MSG_OUTLINE          = 'OLne';
static const uint32 MSG_OUTLINE_TOGGLE   = 'OLtg';
static const uint32 MSG_OUTLINE_UPDATE   = 'OLup';
static const uint32 MSG_OUTLINE_SELECTED = 'OLsl';

/*
 * semantic text editing
 */
static const uint32 MSG_INSERT_ENTITY   = 'Thie';
static const uint32 MSG_ENTITY_SELECTED = 'Tens';
static const uint32 MSG_ADD_HIGHLIGHT   = 'This';

// common message properties
#define MSG_PROP_LABEL "label"
