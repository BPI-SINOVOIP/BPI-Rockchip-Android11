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

import { Component } from '@angular/core';

import { AppService } from "./appservice";


@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.scss']
})
export class AppComponent {
  _sideNavOpened = false;
  get sideNavOpened(): boolean {
    return this._sideNavOpened;
  }
  set sideNavOpened(value: boolean) {
    this._sideNavOpened = value;
    if (!value) {
      this.selectedEntity = this.selectedEntity.slice();
    }
  }
  selectedEntity: {name: string; value: any[]}[] = [];

  constructor(private appService: AppService) {
    appService.closeSideNavEmitter.subscribe(() => {this.sideNavOpened = false});
    appService.showDetailsEmitter.subscribe(
      (entity) => {
        this.selectedEntity.length = 0;
        if (entity) {
          let self = this;
          Object.keys(entity).forEach(function(value){
            if (value !== 'urlsafe_key') {
              self.selectedEntity.push({
                name: value,
                value: (entity[value] instanceof Array) ? entity[value] : [entity[value]]
              });
            }
          });
        }
        this.sideNavOpened = !this.sideNavOpened;
      },
      (error) => {
        console.log(error);
      }
    )
  }
}
