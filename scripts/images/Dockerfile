# docs/images の SVG を PNG にするイメージ。追記した表記は Noto Sans JP の文字なので、
# 同じ字形の Noto Sans CJK を入れておく (SVG の font-family で後ろに指定してある)
FROM debian:bookworm-slim
RUN apt-get update \
 && apt-get install -y --no-install-recommends librsvg2-bin fonts-noto-cjk \
 && rm -rf /var/lib/apt/lists/*
