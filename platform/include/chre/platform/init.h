/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

/**
 * @file CHRE is migrating all platforms away from using chre/core/init.h to
 * doing their own platform-specific initialization. CHRE provides a reference
 * init implementation in chre/platform/shared/init.h which most platforms will
 * use. This file is a temporary measure for platforms that have not yet
 * migrated away from core init and is implemented in
 * chre/platform/shared/init.cc (a wrapper around the reference init
 * implementation).
 *
 * TODO(b/424584443): Delete this file after all platforms have been migrated
 * away from using chre/core/init.h
 */
namespace chre {

/**
 * Initializes all components of core chre that will be used by the platform.
 * Must initialize the EventLoopManagerSingleton.
 */
void platformInit();

/**
 * Performs deinitialization the EventLoopManagerSingleton.
 */
void platformDeinit();

}  // namespace chre
