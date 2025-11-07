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
static const uint32 MSG_SETTINGS  = 'Conf';
// config keys
#define CONF_PANEL_OUTLINE_SHOW     "conf:panel:outline:show"

// file handling
static const uint32 MSG_FILE_NEW  = 'Fnew';
static const uint32 MSG_FILE_OPEN = 'Fopn';
static const uint32 MSG_FILE_SAVE = 'Fsav';

// editing
// strange there is no API definition for this, only a hook method...
static const uint32 MSG_SELECTION_CHANGED = 'SLch';

/*
 * built-in panels
 */

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
