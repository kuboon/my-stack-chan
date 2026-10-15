/**
 * The browser modules, compiled as one graph.
 *
 * Every entrypoint below goes into a single `Deno.bundle({ codeSplitting: true })` call, so a
 * module two of them import is emitted once, into a chunk both import.
 *
 * The islands are globbed rather than listed: an island is a file in a directory, and that is the
 * decision. Every path here is under `client/`: this is the server compiling the browser's half of
 * the site.
 */

import { createAssetServer } from "@remix-kbn/assets-deno";

import { base } from "../client/base.ts";

/** The directory every entrypoint below, and every `clientEntry()` id, is resolved against. */
const clientDir = new URL("../client/", import.meta.url);

/** Where the chunks are served, and where `entryUrl()` resolves against. */
export const assetsPath = `${base}/assets`;

export const assets = await createAssetServer({
  rootDir: decodeURIComponent(clientDir.pathname),
  entrypoints: [
    // The client runtime. Every hydrating page loads this one.
    "hydration.ts",
    // Every island, by where it is rather than by name.
    "islands/*.tsx",
  ],
  basePath: assetsPath,
  mode: "bundle",
  // Source maps would double the file count of a static deploy for no gain.
  bundle: { sourcemap: "none" },
});
