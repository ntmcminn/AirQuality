import { BrowserModule } from '@angular/platform-browser';
import { NgModule } from '@angular/core';
import { FormsModule } from '@angular/forms';
import { HttpModule } from '@angular/http';

import { AppComponent } from './app.component';
import { AqStatsComponent } from './aq-stats/aq-stats.component';
import { AqGraphsComponent } from './aq-graphs/aq-graphs.component';
import { AqStatusComponent } from './aq-status/aq-status.component';
import { AqNavbarComponent } from './aq-navbar/aq-navbar.component';

// Imports for loading & configuring the in-memory web api
import { InMemoryWebApiModule } from 'angular2-in-memory-web-api';
import { InMemoryDataService }  from './in-memory-airquality.service';

import { MaterialModule } from '@angular/material';


@NgModule({
  declarations: [
    AppComponent,
    AqStatsComponent,
    AqGraphsComponent,
    AqStatusComponent,
    AqNavbarComponent
  ],
  imports: [
    BrowserModule,
    FormsModule,
    HttpModule,
    //InMemoryWebApiModule.forRoot(InMemoryDataService),
    [MaterialModule.forRoot()]
  ],
  providers: [],
  bootstrap: [AppComponent]
})
export class AppModule { }
