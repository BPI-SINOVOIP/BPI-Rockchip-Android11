/**
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Angular modules.
import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';
import { RouterModule } from '@angular/router';

// Angular Material modules.
import { MatButtonModule } from '@angular/material/button';
import { MatListModule } from '@angular/material/list';
import { MatToolbarModule } from '@angular/material/toolbar';

// User modules.
import { DictPipe } from '../dict.pipe';
import { NavBarComponent } from './navbar.component';

@NgModule({
  declarations: [
    DictPipe,
    NavBarComponent,
  ],
  imports: [
    BrowserModule,
    MatButtonModule,
    MatToolbarModule,
    MatListModule,
    RouterModule,
  ],
  exports: [
    NavBarComponent,
  ],
  providers: [],
})
export class NavModule { }
