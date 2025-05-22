## API Controls

The application includes a Flask-based web API for remote camera control:

- Start the API server: `python streaming_and_webservices/main.py`
- Access endpoints:
  - `/zoom?multiplier=10.0`: Set zoom level (1.0x to 30.0x)
  - `/icr/toggle?enable=true`: Enable/disable ICR mode