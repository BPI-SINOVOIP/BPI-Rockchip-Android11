# Codelab: Finding UI elements using chrome.automation API

A common task in autotests is to make hardware changes and verify that UI gets
updated or interact with UI elements and verify that hardware is updated. We can
use the [chrome.automation] API to help with both of these tasks.

[TOC]

## Getting familiar with chrome.automation API

Detailed information about chrome.automation API can be found at
https://developer.chrome.com/extensions/automation.

In short, the API is a wrapper around Chrome's hierarchy of accessibility nodes
that describe Chrome UI elements. The most important attributes of accessibility
nodes are **role** and **name**. See the section on [Accessibility Attributes]
of the accessiblity overview.

## Setup

Follow the steps in [Loading autotest extension on
device](loading-autotest-extension-on-device.md). Loading the AutotestPrivate
extension will give you access to chrome.automation API as well.

## To find a specific UI element

Load a js console connected to the autotest extension's background page. See the
previous section for steps on how to connect to the extension's background page.

**NOTE**: The following steps are meant to be run interactively in the console
and will not work if used in a real test. Section [Using chrome.automation in
autotests](#Using-chrome_automation-in-autotests) shows how to use the API
in a real test.

Let's start by grabbing a reference to the root node of the accessibility tree.

``` js
var root;
chrome.automation.getDesktop(r => root = r);
```

### Finding a button in the hierarchy

Let's demonstrate how to simulate a click on the launcher button in the system
shelf.

We'll start by listing all buttons visible in the tree.

``` js
root.findAll({attributes: {role: "button"}}).map(node => node.name);
```

After typing that into the console you should get a response such as this:

``` js
> (7) ["Back", "Launcher", "Chromium", "Stylus tools", "Status tray, time 4:21
PM, Battery is 22% full.", "Connected to Ethernet", "Battery is 22% full. Time
 left until battery is empty, 1 hour and 39 minutes"]
```

**NOTE**: Names will change depending on the locale of the device. We currently
don't have a locale independent way of identifying UI nodes.

Just by looking at button names we can easily guess that the button named
"Launcher" is the one we're looking for.

Finally, to simulate a click on our button:

``` js
var launcher = root.find({attributes: {role: "button", name: "Launcher"}});
launcher.doDefault();
```

The `doDefault` method performs an action based on the node's *role*, which for
buttons is a button click.

The `find` method supports multiple attributes filters. It returns UI elements
that satisfy all conditions.

## Important roles

The API supports interactions with many types of UI elements.

The following table contains chrome.automation roles for common UI elements:

| views class      | chorme.automation role |
|-----------------:|------------------------|
| views::Button    | button                 |
| views::Label     | staticText             |
| views::ImageView | image                  |
| views::TextField | textField              |

## Finding name and role of a view subclass

View subclasses override the `GetAccessibleNodeData` method to provide role and
name information.

For example, look at [views::Button::GetAccessibleNodeData].

## Using chrome.automation in autotests

chrome.automation extension can be accessed through the autotest extension.

``` python
with Chrome.chrome(autotest_ext=True) as cr:
    ext = cr.autotest_ext
    ext.ExecuteJavaScript("""
        chrome.automation.getDesktop(root => {
            var launcher = root.find({attributes: {role: 'button', name: 'Launcher'}});
            launcher.doDefault();
        });
    """)
```

[chrome.automation]: https://developer.chrome.com/extensions/automation
[Accessibility Attributes]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/accessibility/overview.md#the-accessibility-tree-and-accessibility-attributes
[views::Button::GetAccessibleNodeData]: https://cs.chromium.org/search/?q=views::Button::GetAccessibleNodeData
[Getting to a command prompt]: https://www.chromium.org/chromium-os/poking-around-your-chrome-os-device#TOC-Getting-to-a-command-prompt
[Run Chromium with flags]: https://www.chromium.org/developers/how-tos/run-chromium-with-flags
