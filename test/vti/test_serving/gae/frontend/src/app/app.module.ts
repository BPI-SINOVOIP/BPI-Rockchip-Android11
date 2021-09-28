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
import { BrowserAnimationsModule } from '@angular/platform-browser/animations';
import { BrowserModule } from '@angular/platform-browser';
import { FormsModule } from '@angular/forms';
import { HttpClientModule } from '@angular/common/http';
import { NgModule } from '@angular/core';
import { RouterModule, Routes } from '@angular/router';

// Angular Material modules
import { MatButtonModule } from '@angular/material/button';
import { MatCardModule } from '@angular/material/card';
import { MatChipsModule } from '@angular/material/chips';
import { MatExpansionModule } from '@angular/material/expansion';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material';
import { MatInputModule } from '@angular/material/input';
import { MatListModule } from '@angular/material/list';
import { MatPaginatorModule } from '@angular/material/paginator';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSnackBarModule } from '@angular/material/snack-bar';
import { MatSelectModule } from '@angular/material/select';
import { MatSidenavModule } from '@angular/material/sidenav';
import { MatSortModule } from '@angular/material/sort';
import { MatTableModule } from '@angular/material/table';
import { MatTabsModule } from '@angular/material/tabs';

// User components.
import { AppComponent } from './app.component';
import { BuildComponent } from './menu/build/build.component';
import { DashboardComponent } from './menu/dashboard/dashboard.component';
import { DeviceComponent } from './menu/device/device.component';
import { FilterComponent } from './shared/filter/filter.component';
import { JobComponent } from './menu/job/job.component';
import { LabComponent } from './menu/lab/lab.component';
import { ScheduleComponent } from './menu/schedule/schedule.component';

// User modules.
import { NavModule } from './shared/navbar/navbar';

// Other dependencies.
import { FlexLayoutModule } from '@angular/flex-layout';

// User directives for CDK (Component Development Kit).
import { CdkDetailRowDirective } from './menu/cdk-detail-row.directive';


const appRoutes: Routes = [
  { path: 'device', component: DeviceComponent },
  { path: 'build', component: BuildComponent },
  { path: 'job', component: JobComponent },
  { path: 'lab', component: LabComponent },
  { path: 'schedule', component: ScheduleComponent },
  { path: '', component: DashboardComponent },
  { path: '**', redirectTo: '/', pathMatch: 'full' }
];


@NgModule({
  imports: [
    MatButtonModule,
    MatCardModule,
    MatChipsModule,
    MatExpansionModule,
    MatFormFieldModule,
    MatIconModule,
    MatInputModule,
    MatListModule,
    MatPaginatorModule,
    MatProgressSpinnerModule,
    MatSnackBarModule,
    MatSelectModule,
    MatSidenavModule,
    MatSortModule,
    MatTableModule,
    MatTabsModule,
  ],
  exports: [
    MatButtonModule,
    MatCardModule,
    MatChipsModule,
    MatExpansionModule,
    MatFormFieldModule,
    MatIconModule,
    MatInputModule,
    MatListModule,
    MatPaginatorModule,
    MatProgressSpinnerModule,
    MatSnackBarModule,
    MatSelectModule,
    MatSidenavModule,
    MatSortModule,
    MatTableModule,
    MatTabsModule,
  ]
})
export class MaterialModule {}


@NgModule({
  declarations: [
    AppComponent,
    BuildComponent,
    CdkDetailRowDirective,
    DashboardComponent,
    DeviceComponent,
    FilterComponent,
    JobComponent,
    LabComponent,
    ScheduleComponent,
  ],
  imports: [
    BrowserAnimationsModule,
    BrowserModule,
    FlexLayoutModule,
    FormsModule,
    HttpClientModule,
    MaterialModule,
    NavModule,
    RouterModule.forRoot(
      appRoutes
    ),
  ],
  providers: [],
  bootstrap: [AppComponent]
})
export class AppModule { }
