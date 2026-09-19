// Production environment: the Angular app is served from GitHub Pages, which
// cannot proxy /api, so the browser calls the Render backend directly. This
// must match the backend's CORS_ALLOWED_ORIGINS and its onrender.com URL
// (https://<service>-<random>.onrender.com after the blueprint is created).
// The GitHub Pages workflow can override this via the BACKEND_URL repo variable.
export const environment = {
  apiBaseUrl: 'https://quickquill-backend.onrender.com',
};
