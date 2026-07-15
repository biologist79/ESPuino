# ESPuino MediaHub — leichtgewichtiges Image auf Basis von python:3.12-slim
FROM python:3.12-slim

WORKDIR /app

# Abhängigkeiten zuerst (bessere Layer-Caching-Ausnutzung)
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Anwendung
COPY . .

ENV MEDIAHUB_DATA=/data
EXPOSE 8080

# 2 Worker reichen für ein Heim-Setup dicke aus
CMD ["gunicorn", "--bind", "0.0.0.0:8080", "--workers", "2", "app:app"]
