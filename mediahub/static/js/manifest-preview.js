// Toggles an inline, pretty-printed JSON preview of a card's manifest
// (the "Manifest" button on the cards list) — fetched once, then cached.
(function () {
	"use strict";

	document.addEventListener("DOMContentLoaded", function () {
		document.querySelectorAll(".mh-manifest-btn").forEach(function (btn) {
			var cardId = btn.dataset.cardId;
			var row = document.querySelector('.mh-manifest-row[data-card-id="' + cardId + '"]');
			if (!row) {
				return;
			}
			var pre = row.querySelector(".mh-manifest-json");
			var loaded = false;

			btn.addEventListener("click", function () {
				if (row.hidden && !loaded) {
					pre.textContent = "…";
					fetch(btn.dataset.url).then(function (resp) {
						if (!resp.ok) {
							throw new Error("manifest fetch failed");
						}
						return resp.json();
					}).then(function (data) {
						pre.textContent = JSON.stringify(data, null, 2);
						loaded = true;
					}).catch(function () {
						pre.textContent = btn.dataset.error;
					});
				}
				row.hidden = !row.hidden;
			});
		});
	});
})();
