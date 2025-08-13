let defaultVroot;

if (window.navigator.userAgent.includes('Electron')) {
    // Electron environment
    defaultVroot = "/virtual/";
} else if (window.location.hostname.includes("localhost")) {
    // Local development
    defaultVroot = "/dist/virtual/";
} else {
    // GitHub Pages or other hosting
    defaultVroot = "/em/virtual/";
}

window.configurationDefaults = {
    vroot: defaultVroot,
    boot:  `${defaultVroot}boot.html`,
    worker:`${defaultVroot}worker.js`,
    droot: "/",
    env: {},
};

window.configurationTab = {
    type: 'configuration',
    name: 'Configuration',
    path: null,
    unsaved: false,
    configuration: {
        ...window.configurationDefaults
    },
};

async function loadConfiguration() {
    const storedConfiguration = localStorage
        .getItem('em-configuration');

    if (!storedConfiguration) {
        console.log("loading default configuration");
        return window.configurationTab.configuration;
    }

    let parsedConfiguration;

    try {
        parsedConfiguration = JSON.parse(storedConfiguration);
    } catch (e) {
        window.updateStatus(
            "Parse error in stored configuration", "error");
        return window.configurationTab.configuration;
    }

    if (typeof Module !== "undefined" && parsedConfiguration["env"]) {
        Module.environ(parsedConfiguration["env"]);
    }

    return window.configurationTab.configuration = parsedConfiguration;
}

async function storeConfiguration(configuration) {
    try {
        const json = JSON.stringify(configuration);

        if (!json) {
            window.updateStatus("Nothing to store", "error");
            return;
        }

        localStorage.setItem(
            "em-configuration", json);
        window.updateStatus("Stored configuration");

        if (typeof Module !== "undefined" && configuration["env"]) {
            Module.environ(configuration["env"]);
        }

        return true;
    } catch (e) {
        window.updateStatus(
            "Parse error while storing configuration", "error");
        return false;
    }
}

window.loadConfigurationForm = async function(container) {
    const form = container.querySelector("#configuration-form");
    const configuration = await loadConfiguration();

    for (let element of form.elements) {
        let item = element.name;

        if (typeof form[item] === 'undefined') {
            continue;
        }

        if (typeof configuration[item] === 'undefined') {
            form[item].value = null;

            continue;
        }

        if (element.hasAttribute("json")) {
            if (configuration[item]) {
                form[item].value =
                    JSON.stringify(configuration[item], null, 4);
            } else {
                form[item].value = null;
            }
            continue;
        }

        form[item].value = configuration[item];
    }
};

window.setUpConfigurationContainer = async function(container) {
    const form = container.querySelector("#configuration-form");
    
    container.querySelector(
        "#configuration-save"
    ).addEventListener("click", (event) => {
        event.preventDefault();

        for (let element of form.elements) {
            let value = element.value;

            if (element.hasAttribute("required") && !value) {
                window.updateStatus(
                    `Please provide ${element.name}`, "error");
                return;
            }

            if (element.hasAttribute("json") && value) {
                try {
                    value = JSON.parse(value);
                } catch (e) {
                    window.updateStatus(
                        `Parse error in ${element.name}`, "error");
                    return;
                }
            }

            window.configurationTab.configuration[element.name] = value;
        }

        if (window.storeConfiguration(
            window.configurationTab.configuration)) {
                window.closeTabView(window.configurationTab);
        }
    });

    container.querySelector(
        "#configuration-reset"
    ).addEventListener("click", async (event) => {
        event.preventDefault();
        await window.storeConfiguration(
            window.configurationDefaults);
        await window.loadConfigurationForm(container);
    });

    await window.loadConfigurationForm(container);
};

window.updateConfigurationContainer = async function(container, tab) {
    if (tab.type != "configuration") {
        container.style.display = "none";
        return;
    }

    await window.loadConfigurationForm(container);
    container.style.display = "block";
};