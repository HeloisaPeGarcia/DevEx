FROM node:22-alpine

WORKDIR /app
COPY package.json package.json
COPY server.js server.js

RUN npm install --omit=dev

EXPOSE 8080
CMD ["node", "server.js"]

