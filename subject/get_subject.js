const { chromium } = require('playwright');
const fs = require('fs');

function launchBrowser(options) {
  const executablePath = process.env.JARVIS_CHROMIUM_EXECUTABLE_PATH;
  return chromium.launch({
    ...options,
    ...(executablePath ? { executablePath } : {})
  });
}

const USER_FIELDS = 'input[type="email"], input[name="username"], '
  + 'input[name="login"], input[name="email"], input[name="loginfmt"], '
  + 'input[autocomplete="username"], input#username, input[type="text"]';
const PASSWORD_FIELDS = 'input[type="password"]';

// First field of the page that matches the selector and is displayed
async function firstVisible(page, selector) {
  try {
    for (const field of await page.locator(selector).all()) {
      if (await field.isVisible())
        return field;
    }
  } catch (error) {
    // The page is navigating : nothing to see for now
  }
  return null;
}

// Fill the login form of the forge (single page or username then password).
// Returns true when the forge is reached again, false when the password is
// refused or the form is never found.
async function consoleLogin(page, url, username, password) {
  const forgeHost = new URL(url).host;
  const onForge = () => new URL(page.url()).host === forgeHost;
  const deadline = Date.now() + 90000;
  let passwordSent = false;

  while (Date.now() < deadline) {
    await page.waitForLoadState('domcontentloaded').catch(() => {});

    const passwordField = await firstVisible(page, PASSWORD_FIELDS);

    if (!passwordField) {
      if (onForge()) {
        await page.waitForLoadState('networkidle', { timeout: 5000 }).catch(() => {});
        return true;
      }

      const userField = await firstVisible(page, USER_FIELDS);
      if (userField) {
        await userField.fill(username);
        await userField.press('Enter');
        await page.waitForTimeout(1500);
      } else {
        await page.waitForTimeout(500);
      }
      continue;
    }

    // The password is asked again after it was sent : it was refused
    if (passwordSent) {
      await page.waitForTimeout(1500);
      if (await firstVisible(page, PASSWORD_FIELDS))
        return false;
      continue;
    }

    // Username and password on the same page
    const userField = await firstVisible(page, USER_FIELDS);
    if (userField && !(await userField.inputValue()))
      await userField.fill(username);

    await passwordField.fill(password);
    await passwordField.press('Enter');
    passwordSent = true;

    await page.waitForURL((address) => address.host === forgeHost,
      { timeout: 15000 }).catch(() => {});
  }

  return false;
}

(async () => {
  const mode = process.argv[2];
  const url = process.argv[3];
  const subject_path = process.argv[4];
  const file_name = process.argv[5] ?? null;

  let other_path = process.argv[6] ?? subject_path;
  if (other_path.trim() === ""){
      other_path = subject_path;
  }



  if (!mode || !url || !subject_path) {
    console.error("Usage: node get_subject.js <auth|run|download> <url>");
    process.exit(1);
  }

  const auth_path = subject_path + '/auth.json';
  let subject_html_path = other_path + '/subject.html';
  if (file_name != null)
    subject_html_path = other_path + '/' + file_name;

  // Start the Auth window
  if (mode === "auth") {
    // Jarvis shows the instructions (connect, then close the window) when it
    // reads this line, just before the window opens
    console.log("JARVIS_AUTH_WINDOW");

    const browser = await launchBrowser({ headless: false });
    const context = await browser.newContext();
    const page = await context.newPage();

    await page.goto(url);

    page.on('close', async () => {
      await context.storageState({ path: auth_path });
      console.log("Auth saved !");

      await browser.close();
      process.exit(0);
    });
  }

  // Sign in without any window : the username (line 1) and the password
  // (line 2) are given on the standard input. Exit code : 0 success | 2 wrong
  // credentials | 1 any other failure.
  else if (mode === "auth-console") {
    const lines = fs.readFileSync(0, 'utf-8').split('\n');
    const username = (lines[0] ?? '').replace(/\r$/, '');
    const password = (lines[1] ?? '').replace(/\r$/, '');
    if (!username || !password) {
      console.error("ERROR Missing username or password");
      process.exit(1);
    }

    const browser = await launchBrowser({ headless: true });
    let logged = false;
    try {
      const context = await browser.newContext();
      const page = await context.newPage();
      await page.goto(url, { waitUntil: 'domcontentloaded' });

      logged = await consoleLogin(page, url, username, password);
      if (logged)
        await context.storageState({ path: auth_path });
    } finally {
      await browser.close();
    }
    process.exit(logged ? 0 : 2);
  }

  // Run and load the subject
  else if (mode === "run") {
    if (!fs.existsSync(auth_path)) {
      console.error("ERROR Impossible to find auth.json, please auth before");
      process.exit(1);
    }

    const browser = await launchBrowser({ headless: true });
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

    const browser = await launchBrowser({ headless: true });
    const context = await browser.newContext({
      storageState: auth_path
    });

    const request = context.request;

    try {
      const response = await request.get(url);

      // No file to download
      if (response.status() === 404) {
        // console.error("No given file for this TP.");
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

      // console.log(`\x1b[32mGiven files downloaded.\x1b[0m`);

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

})().catch((error) => {
  console.error("ERROR Browser automation failed:", error.message);
  process.exit(1);
});