FROM node:25.0.0-alpine AS build

WORKDIR /app

COPY package.json package-lock.json /app

RUN npm ci --no-audit --no-fund

RUN npm install -g serve

COPY index.html vite.config.js svelte.config.js jsconfig.json /app
COPY public /app/public
COPY src /app/src

RUN npm run build

CMD ["serve", "-s", "-l", "8080", "dist"]
