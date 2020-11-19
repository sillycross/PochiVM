#!/bin/bash
python3 pochivm-build update-docker-image && python3 pochivm-build cmake production && python3 pochivm-build make production

