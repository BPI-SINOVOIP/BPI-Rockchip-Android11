/** Convert chrome tabs API to promise */
const chromeTabs = {
  get: function(tabId) {
    return new Promise(function(resolve) {
      chrome.tabs.get(tabId, resolve);
    });
  },

  sendMessage: function(tabId, message, options) {
    return new Promise(function(resolve) {
      chrome.tabs.sendMessage(tabId, message, options, resolve);
    });
  },
};

class TaskMonitor {
  constructor() {
    // Bind the callback
    this._updated = this._updated.bind(this);
    this._records = [];
  }

  bind() {
    chrome.processes.onUpdated.addListener(this._updated);
  }

  unbind() {
    chrome.processes.onUpdated.removeListener(this._updated);
  }

  _updated(processes) {
    // Structure to hold synthesized data
    const data = {
      timestamp: Date.now(),
      processes: [],
      tabInfo: [],
    };

    // Produce a set of tabId's to populate data.tabInfo in a separate loop
    const tabset = new Set();

    // Only take a subset of the process data
    for (const process of Object.values(processes)) {
      // Skip processes that don't consume CPU
      if (process.cpu == 0) {
        continue;
      }

      // Only take a subset of the data
      const currProcess = {
        pid: process.osProcessId,
        network: process.network,
        cpu: process.cpu,
        tasks: [],
      };

      // Populate process info with 'tabId' and 'title' information
      for (const task of Object.values(process.tasks)) {
        if ('tabId' in task) {
          tabset.add(task.tabId);
          currProcess.tasks.push({
            tabId: task.tabId,
            title: task.title,
          });
        } else {
          if ('title' in task) {
            currProcess.tasks.push({title: task.title});
          }
        }
      }
      data.processes.push(currProcess);
    }

    // Populate data.tabInfo with unique tabId's
    for (const tabId of tabset) {
      data.tabInfo.push({tabId: tabId});
    }

    const promises = [];

    // Record url, audio, muted, and video count
    // information per currently open tab
    for (const tabInfo of data.tabInfo) {
      promises.push(
          chromeTabs.get(tabInfo.tabId).then((tab) => {
            tabInfo.title = tab.title;
            tabInfo.url = tab.url;
            tabInfo.audio_played = !!tab.audible;
            tabInfo.muted = tab.mutedInfo.muted;
          })
      );

      promises.push(
          chromeTabs.sendMessage(tabInfo.tabId, 'numberOfVideosPlaying')
              .then((n) => {
                if (typeof n === 'undefined') {
                  tabInfo.videos_playing = 0;
                  return;
                } else {
                  tabInfo.videos_playing = n;
                }
              })
      );
    }

    Promise.all(promises).finally(() => {
      this._records.push(data);

      if (this._records.length >= 1) {
        this._send();
      }
    });
  }

  _send() {
    const formData = new FormData();
    this._records.forEach(function(record, index) {
      formData.append(index, JSON.stringify(record));
    });
    this._records.length = 0;

    const url = 'http://localhost:8001/task-monitor';
    const req = new XMLHttpRequest();
    req.open('POST', url, true);
    req.send(formData);
  }
}

this.task_monitor = new TaskMonitor();
