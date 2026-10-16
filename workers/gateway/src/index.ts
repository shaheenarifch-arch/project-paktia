export interface Env {
  GAME_ENV: string;
  PUBLIC_GAME_VERSION: string;
  GAME_ASSETS: R2Bucket;
  GAME_JOBS: Queue;
}

const json = (body: unknown, status = 200) =>
  new Response(JSON.stringify(body), {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "cache-control": "no-store"
    }
  });

export default {
  async fetch(request: Request, env: Env): Promise<Response> {
    const url = new URL(request.url);

    if (request.method === "GET" && url.pathname === "/health") {
      return json({
        ok: true,
        service: "project-paktia-gateway",
        environment: env.GAME_ENV,
        version: env.PUBLIC_GAME_VERSION
      });
    }

    if (request.method === "GET" && url.pathname === "/api/v1/game/config") {
      return json({
        game: "Project Paktia",
        map: "gardun_city",
        mode: "stronghold_assault",
        version: env.PUBLIC_GAME_VERSION
      });
    }

    return json({ error: "not_found" }, 404);
  }
};
