import { readdir, readFile } from "node:fs/promises";
import { join } from "node:path";

async function walk(directory) {
  const entries = await readdir(directory, { withFileTypes: true });

  for (const entry of entries) {
    const path = join(directory, entry.name);

    if (entry.isDirectory()) {
      await walk(path);
    } else if (entry.name.endsWith(".json")) {
      JSON.parse(await readFile(path, "utf8"));
      console.log(`valid: ${path}`);
    }
  }
}

await walk("data");
