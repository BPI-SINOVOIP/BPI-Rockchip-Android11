chrome.runtime.onMessage.addListener((msg, sender, sendResponse) => {
    if (msg.text === 'title') {
        sendResponse(document.title);
    }
});
