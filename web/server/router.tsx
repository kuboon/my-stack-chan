/**
 * The site, wired by hand.
 *
 * Route definitions live in `client/routes.ts` and this file maps them to what answers them.
 * What is exported is a plain `@remix-run/fetch-router` router:
 *
 * - **Locally** `deno serve router.tsx` answers every page. This is also what the tests run
 *   against.
 * - **GitHub Pages** is `@remix-kbn/ssg` crawling this same object with `fetch()` and writing each
 *   response to disk, starting from `entryPoints` and following links.
 */

import {
  createController,
  createRouter,
  type RouterContext,
} from "@remix-run/fetch-router";
import { render } from "@remix-run/render-middleware";
import { createFileTree, githubPages } from "@remix-kbn/ssg/site";
import type { FileServerBehavior } from "@remix-kbn/ssg/site";

import { assets, assetsPath } from "./assets.ts";
import { clientRuntime } from "./runtime.ts";
import { ogImage, ogPaths, serveOgImage } from "./og/mod.ts";
import { base } from "../client/base.ts";
import { Layout, type PageModule } from "../client/layout.tsx";
import { routes } from "../client/routes.ts";

import { docsController } from "./docs/mod.tsx";
import * as Home from "../client/pages/index.tsx";

/** Deploy path prefix. The build strips it back off when writing, so output lands at the root. */
export { base };

/** Where the static build deploys. The build writes the file this rule would serve. */
export const fileServer: FileServerBehavior = githubPages();

/**
 * Renders a page module into the shell.
 *
 * The route comes in alongside the module because the page's own path is what its social card is
 * registered under.
 */
function pageAction(route: { href(): string }, page: PageModule) {
  const image = ogImage(route.href(), page);
  const Page = page.default;

  return (context: AppContext): Response =>
    context.render(
      <Layout
        title={page.title}
        description={page.description}
        image={image}
        script={page.hydrate ? clientRuntime : null}
      >
        <Page />
      </Layout>,
    );
}

/** The files under `client/static/`, served verbatim at their own names. */
const staticFiles = await createFileTree({
  rootDir: `${import.meta.dirname}/../client/static`,
  basePath: `${base}/static`,
  cacheControl: "public, max-age=3600",
});

/** `render({ assets })` puts `context.render(node)` on every request. */
const router = createRouter({ middleware: [render({ assets })] });

/** The request context those middlewares produce — `context.render`, in practice. */
export type AppContext = RouterContext<typeof router>;

declare module "@remix-run/fetch-router" {
  interface RouterTypes {
    context: AppContext;
  }
}

const top = createController(routes, {
  actions: {
    home: pageAction(routes.home, Home),
  },
});

router.map(routes, top);
// Both docs routes at once: the listing, and one document.
router.map(routes.docs, docsController);

router.get(`${base}/static/*path`, ({ request }) => staticFiles.fetch(request));
router.get(`${assetsPath}/*path`, ({ request }) => assets.fetch(request));
router.get(`${base}/og/*path`, ({ request }) => serveOgImage(request));

/**
 * Where the static crawl starts.
 *
 * Pages are reached by following links from `/`. The social cards are the exception: an
 * `og:image` is an absolute URL meant for someone else's server, so nothing links to them.
 */
export const entryPoints: readonly string[] = ["/", ...ogPaths()];

export default router;
