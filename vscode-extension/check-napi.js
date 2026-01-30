const { app } = require('electron');
app.whenReady().then(() => {
    console.log('N-API version:', process.versions.napi);
    console.log('Node version:', process.versions.node);
    console.log('All versions:', JSON.stringify(process.versions, null, 2));
    app.quit();
});
