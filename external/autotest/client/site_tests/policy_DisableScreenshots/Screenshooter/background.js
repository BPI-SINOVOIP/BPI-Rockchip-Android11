function setTitle(title) {
    chrome.tabs.executeScript({
        code: 'document.title = "' + title + '"'
    });
}

chrome.commands.onCommand.addListener((command) => {
    if (command === 'activeTab') {
        chrome.tabs.query({active: true, currentWindow: true}, (tabs) => {
            chrome.tabs.sendMessage(tabs[0].id, {text: 'title'}, (method) => {
                if (method === 'captureVisibleTab') {
                    chrome.tabs.captureVisibleTab((img) => {
                        setTitle(img);
                    });
                } else if (method === 'tabCapture') {
                    chrome.tabCapture.capture({video: true}, (stream) => {
                        setTitle(stream);
                    });
                } else if (method === 'desktopCapture') {
                    chrome.desktopCapture.chooseDesktopMedia(
                        ['screen', 'window', 'tab'], (streamId) => {
                            setTitle(streamId);
                        }
                    );
                }
            });
        });
    }
});
