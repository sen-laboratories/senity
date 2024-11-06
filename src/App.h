/*
 * Copyright 2024, Gregor B. Rosenauer <gregor.rosenauer@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <Application.h>


class App : public BApplication
{
public:
							App();
	virtual					~App();

	virtual void			AboutRequested();

private:
};

