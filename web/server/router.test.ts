import { assert, assertEquals, assertStringIncludes } from "@std/assert";
import router from "./router.tsx";

Deno.test("GET / returns the shell with nav links", async () => {
  const res = await router.fetch(new Request("http://x/"));
  assertEquals(res.status, 200);
  assertStringIncludes(res.headers.get("content-type") ?? "", "text/html");
  const html = await res.text();
  assertStringIncludes(html, "<!DOCTYPE html>");
  assertStringIncludes(html, "my-stack-chan");
  assertStringIncludes(html, 'href="/docs"');
  // The home page places no island, so it ships no runtime.
  assert(!html.includes('<script type="module"'));
});

for (
  const [path, needle] of [
    ["/docs", "Docs"],
    ["/docs/concept", "構想"],
  ] as const
) {
  Deno.test(`GET ${path} renders inside the shell`, async () => {
    const res = await router.fetch(new Request(`http://x${path}`));
    assertEquals(res.status, 200);
    const html = await res.text();
    assertStringIncludes(html, "<main");
    assertStringIncludes(html, needle);
  });
}

Deno.test("GET /docs/unknown is a 404", async () => {
  const res = await router.fetch(new Request("http://x/docs/nope"));
  assertEquals(res.status, 404);
  await res.body?.cancel();
});
