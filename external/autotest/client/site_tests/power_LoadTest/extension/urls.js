// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// List of tasks to accomplish
var URLS = new Array();

var ViewGDoc = ('https://docs.google.com/document/d/');

var BBC_AUDIO_URL = 'https://www.bbc.co.uk/radio/player/bbc_world_service';

var PLAY_MUSIC_URL = 'https://play.google.com/music/listen?u=0#/wst/st/a2be2d85-0ac9-3a7a-b038-e221bb63ef71';

function isMP3DecoderPresent() {
    return window['MediaSource'] &&
      window['MediaSource'].isTypeSupported('audio/mpeg');
}

var tasks = [
  {
    // Chrome browser window 1. This window remains open for the entire test.
    type: 'window',
    name: 'background',
    start: 0,
    duration: minutes(60),
    focus: false,
    tabs: [
     'https://www.google.com/search?q=google',
     'https://news.google.com',
     'https://www.reddit.com',
     'https://www.amazon.com',
     'https://www.facebook.com/facebook'
    ]
  },
  {
    // Page cycle through popular external websites for 36 minutes
    type: 'cycle',
    name: 'web',
    start: seconds(1),
    duration: minutes(36),
    delay: seconds(60), // A minute on each page
    timeout: seconds(30),
    focus: true,
    urls: URLS,
  },
  {
    // After 36 minutes, actively read e-mail for 12 minutes
    type: 'cycle',
    name: 'email',
    start: minutes(36) + seconds(1),
    duration: minutes(12) - seconds(1),
    delay: minutes(5), // 5 minutes between full gmail refresh
    timeout: seconds(30),
    focus: true,
    urls: [
       'https://gmail.com',
       'https://mail.google.com'
    ],
  },
  {
    // After 36 minutes, start streaming audio (background tab), total playtime
    // 12 minutes
    type: 'cycle',
    name: 'audio',
    start: minutes(36),
    duration: minutes(12),
    delay: minutes(12),
    timeout: seconds(30),
    focus: false,
    // Google Play Music requires MP3 decoder for playing music.
    // Fall back to BBC if the browser does not have MP3 decoder bundle.
    urls: isMP3DecoderPresent() ? [BBC_AUDIO_URL, BBC_AUDIO_URL] :
                                  [BBC_AUDIO_URL, BBC_AUDIO_URL]
  },
  {
    // After 48 minutes, play with Google Docs for 6 minutes
    type: 'cycle',
    name: 'docs',
    start: minutes(48),
    duration: minutes(6),
    delay: minutes(1), // A minute on each page
    timeout: seconds(30),
    focus: true,
    urls: [
       ViewGDoc + '1ywpQGu18T9e2lB_QVMlihDqiF0V5hsYkhlXCfu9B8jY',
       ViewGDoc + '12qBD7L6n9hLW1OFgLgpurx7WSgDM3l01dU6YYU-xdXU'
    ],
  },
  {
    // After 54 minutes, watch Big Buck Bunny for 6 minutes
    type: 'window',
    name: 'video',
    start: minutes(54),
    duration: minutes(6),
    focus: true,
    tabs: [
        'https://www.youtube.com/embed/YE7VzlLtp-4?start=236&vq=hd720&autoplay=1'
    ]
  },
];


// Updated April 15, 2019.
// 50 entries are determined by taking the top 50 websites from Alexa rankings,
// https://www.alexa.com/topsites/countries/US.
// Similar Web rankings are used to fill in the remainder after some of the
// Alexa rankings are removed.
// https://www.similarweb.com/top-websites/united-states
// NSFW, effective duplicates, and mobile sites are
// left out. Links are changed to focus on content instead of bare login/landing
// pages (when possible).
var u_index = 0;
URLS[u_index++] = 'https://www.google.com/search?q=google';
URLS[u_index++] = 'https://www.youtube.com';
URLS[u_index++] = 'https://www.facebook.com/facebook';
URLS[u_index++] = 'https://www.amazon.com';
URLS[u_index++] = 'https://www.wikipedia.org/wiki/Google';
URLS[u_index++] = 'https://www.reddit.com';
URLS[u_index++] = 'https://www.yahoo.com';
URLS[u_index++] = 'https://www.twitter.com/google';
URLS[u_index++] = 'https://www.linkedin.com/jobs/management-jobs';
URLS[u_index++] = 'https://www.instagram.com/instagram';
URLS[u_index++] = 'https://www.ebay.com';
URLS[u_index++] = 'https://www.netflix.com';
URLS[u_index++] = 'https://www.twitch.tv';
URLS[u_index++] = 'https://www.espn.com';
URLS[u_index++] = 'https://www.instructure.com';
URLS[u_index++] = 'https://www.live.com';
URLS[u_index++] = 'https://www.craigslist.org';
URLS[u_index++] = 'https://www.imgur.com';
URLS[u_index++] = 'https://www.chase.com';
URLS[u_index++] = 'https://www.paypal.com';
URLS[u_index++] = 'https://www.bing.com/search?q=google';
URLS[u_index++] = 'https://www.cnn.com';
URLS[u_index++] = 'https://www.fandom.com';
URLS[u_index++] = 'https://www.imdb.com';
URLS[u_index++] = 'https://www.pinterest.com';
URLS[u_index++] = 'https://www.office.com';
URLS[u_index++] = 'https://www.nytimes.com';
URLS[u_index++] = 'https://www.github.com/explore';
URLS[u_index++] = 'https://www.hulu.com';
URLS[u_index++] = 'https://www.zillow.com';
URLS[u_index++] = 'https://www.microsoft.com';
URLS[u_index++] = 'https://www.apple.com';
URLS[u_index++] = 'https://www.intuit.com';
URLS[u_index++] = 'https://www.salesforce.com';
URLS[u_index++] = 'https://www.stackoverflow.com';
URLS[u_index++] = 'https://www.yelp.com';
URLS[u_index++] = 'https://www.walmart.com';
URLS[u_index++] = 'https://www.bankofamerica.com';
URLS[u_index++] = 'https://www.tumblr.com/explore';
URLS[u_index++] = 'https://www.dropbox.com';
URLS[u_index++] = 'https://www.wellsfargo.com';
URLS[u_index++] = 'https://www.quora.com';
URLS[u_index++] = 'https://www.quizlet.com';
URLS[u_index++] = 'https://www.weather.com';
URLS[u_index++] = 'https://www.accuweather.com';
URLS[u_index++] = 'https://www.foxnews.com';
URLS[u_index++] = 'https://www.msn.com';
URLS[u_index++] = 'https://www.indeed.com/l-Mountain-View-jobs.html';
URLS[u_index++] = 'https://duckduckgo.com/?q=google';
URLS[u_index++] = 'https://www.accuweather.com';
