// Service worker for the ESPuino PWA.
// Purpose: when the player is powered off and the app is launched from the
// home screen, page loads fail and iOS shows a black screen. This worker
// serves a cached offline page for failed navigations instead.

const CACHE = 'espuino-offline-v1';
const OFFLINE_URL = '/offline.html';

self.addEventListener('install', (event) => {
	event.waitUntil(
		caches.open(CACHE).then((cache) => cache.add(new Request(OFFLINE_URL, { cache: 'reload' })))
	);
	self.skipWaiting();
});

self.addEventListener('activate', (event) => {
	event.waitUntil(
		caches.keys().then((keys) => Promise.all(
			keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))
		)).then(() => self.clients.claim())
	);
});

self.addEventListener('fetch', (event) => {
	const req = event.request;
	// Only intervene for top-level page navigations. Everything else (assets,
	// websocket upgrades, API calls) is left to the network as usual.
	if (req.mode !== 'navigate') {
		return;
	}
	event.respondWith(
		fetch(req).catch(() => caches.match(OFFLINE_URL, { ignoreSearch: true }))
	);
});
