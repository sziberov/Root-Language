let electron = require('electron'),
	app = electron.app,
	BrowserWindow = electron.BrowserWindow,
	path = require('path');

app.on('ready', () => {
	let modal = new BrowserWindow({
		width: 1024,
		height: 768,
		webPreferences: {
            nodeIntegration: true,
            nodeIntegrationInWorker: true,
            nodeIntegrationInSubFrames: true,
            enableRemoteModule: true,
            contextIsolation: false
        }
	});

	modal.loadFile(path.join(__dirname, 'Dashboard.html'));
});