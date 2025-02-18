# syntax=docker/dockerfile:1
FROM ubuntu:latest
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update
RUN apt-get install -y zip make cmake gcc-10 g++-10 libcurl4-openssl-dev libssl-dev libcrypto++-dev libboost-iostreams-dev libboost-filesystem-dev libboost-system-dev libboost-test-dev libfcgi-dev spawn-fcgi nginx vim wget git curl
