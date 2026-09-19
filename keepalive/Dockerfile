# Tiny Docker image for the QuickQuill keep-alive cron job.
FROM python:3.12-slim
COPY keepalive.py /usr/local/bin/keepalive.py
ENTRYPOINT ["python3", "/usr/local/bin/keepalive.py"]