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

namespace chre {

/**
 * Performs initialization of CHRE common functionality. This involves the
 * following:
 *
 *  1. SystemTime::init()
 *  2. Construct the *Manager objects accepted in the EventLoopManager
 *     constructor.
 *  3. EventLoopManagerSingleton::init()
 */
void initCommon();

/**
 * Performs deinitialization of CHRE common functionality. This will deinit the
 * EventLoopManagerSingleton and the *Manager objects passed into it.
 */
void deinitCommon();

}  // namespace chre
