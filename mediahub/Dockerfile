# ESPuino MediaHub — lightweight image based on python:3.12-slim
FROM python:3.12-slim

WORKDIR /app

# tzdata so the TZ env var (set in docker-compose.yml) actually resolves —
# python:3.12-slim doesn't ship the zoneinfo database by default.
RUN apt-get update \
	&& apt-get install --no-install-recommends -y tzdata \
	&& rm -rf /var/lib/apt/lists/*

# Dependencies first (better layer cache reuse)
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Application
COPY . .

# Compile the .po translation catalogs into .mo (pybabel ships with the
# Babel package, already installed as a Flask-Babel dependency above).
RUN pybabel compile -d translations

ENV MEDIAHUB_DATA=/data
ENV MEDIAHUB_MEDIA=/media
EXPOSE 8080

# 2 workers are plenty for a home setup. gunicorn's sync worker blocks for
# the whole file transfer during /media/... downloads (sendfile()) - an
# ESPuino on WiFi is much slower than a local client, so the 30s default
# --timeout killed the worker mid-download every time. 600s is generous
# enough for a large audiobook file even over a slow connection.
# --access-logfile - sends every request (manifest fetches, file downloads,
# admin UI) to stdout, so `docker logs` shows them - there's no access
# logging at all otherwise, Flask doesn't log requests under gunicorn.
CMD ["gunicorn", "--bind", "0.0.0.0:8080", "--workers", "2", "--timeout", "600", "--access-logfile", "-", "app:app"]
