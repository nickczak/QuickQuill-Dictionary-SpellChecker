// Production environment: the Angular app is served from GitHub Pages under a
// custom domain (https://quickquill.ink), which cannot proxy /api, so the
// browser calls the Render backend directly at its own custom domain
// (https://api.quickquill.ink). The GitHub Pages workflow can override this
// via the BACKEND_URL repo variable.
export const environment = {
  apiBaseUrl: 'https://api.quickquill.ink',
};
