/**
 * Every URL the site answers, in one place.
 *
 * `server/router.tsx` maps these to the controllers that answer them, and everything that links
 * reads `routes.docs.index.href()` rather than rebuilding `${base}/docs` at each call site.
 *
 * The map is built with the deploy prefix as its base, so hrefs are correct under a GitHub Pages
 * repo sub-path or a PR preview URL without anyone prepending anything.
 */

import { get, route } from "@remix-run/fetch-router/routes";

import { base } from "./base.ts";

export const routes = route(base, {
  home: get("/"),
  docs: route("docs", {
    index: get("/"),
    /** One document. Documents are Markdown files; `server/docs/` answers this by reading the slug's file. */
    show: get("/:slug"),
  }),
});
