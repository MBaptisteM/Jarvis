const { chromium } = require('playwright');
const fs = require('fs');

(async () => {
  const mode = process.argv[2];
  const url = process.argv[3];
  const subject_path = process.argv[4];

  if (!mode || !url || !subject_path) {
    console.error("Usage: node get_subject.js <login|run|download> <url>");
    process.exit(1);
  }

  const auth_path = subject_path + '/auth.json';
  const subject_html_path = subject_path + '/subject.html';

  // Start the Login window
  if (mode === "login") {
    console.log("\n➡️ \x1b[1mPlease \x1b[32mconnect to your forge account.\x1b[0m");
    console.log("➡️ \x1b[1mThen close the window.\x1b[0m");

    const browser = await chromium.launch({ headless: false });
    const context = await browser.newContext();
    const page = await context.newPage();

    await page.goto(url);

    page.on('close', async () => {
      await context.storageState({ path: auth_path });
      console.log("Login saved !");

      await browser.close();
      process.exit(0);
    });
  }

  // Run and load the subject
  else if (mode === "run") {
    if (!fs.existsSync(auth_path)) {
      console.error("ERROR Impossible to find auth.json, please auth before");
      process.exit(1);
    }

    const browser = await chromium.launch({ headless: true });
    const context = await browser.newContext({
      storageState: auth_path
    });

    const page = await context.newPage();
    await page.goto(url);

    const content = await page.content();

    // Write the content in subject.html
    fs.writeFileSync(subject_html_path, content, 'utf-8');

    await browser.close();
  }


  // Download the file if it exist (call only with .gz pages)
  else if (mode === "download") {
    if (!fs.existsSync(auth_path)) {
      console.error("ERROR Impossible to find auth.json, please auth before");
      process.exit(1);
    }

    const browser = await chromium.launch({ headless: true });
    const context = await browser.newContext({
      storageState: auth_path
    });

    const request = context.request;

    try {
      const response = await request.get(url);

      // No file to download
      if (response.status() === 404) {
        console.error("No given file for this TP.");
        await browser.close();
        process.exit(1);
      }

      // HTTP error handling
      if (!response.ok()) {
        console.error(`HTTP ERROR : ${response.status()}`);
        await browser.close();
        process.exit(-1);
      }

      // Get the content
      const buffer = await response.body();

      const fileName = url.split('/').pop() || 'file.gz';
      const filePath = `${subject_path}/${fileName}`;

      fs.writeFileSync(filePath, buffer);

      console.log(`\x1b[32mGiven files downloaded.\x1b[0m`);

    } catch (err) {
      console.error("ERROR Impossible to get the given files :", err.message);
      process.exit(-1);
    }

    await browser.close();
  }

  else {
    console.error("ERROR Unknown mode :", mode);
    process.exit(1);
  }

})();