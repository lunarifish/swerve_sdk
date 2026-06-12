import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { execSync } from 'child_process'

// 获取当前 git commit short hash
let commitId = 'dev'
try {
  commitId = execSync('git rev-parse --short HEAD', { encoding: 'utf-8' }).trim()
} catch {
  // 非 git 环境（如部署包）fallback 为 'dev'
}

// https://vite.dev/config/
export default defineConfig({
  plugins: [react()],
  define: {
    __COMMIT_ID__: JSON.stringify(commitId),
  },
  server: {
    watch: {
      usePolling: true,
      interval: 1000,
    },
  },
})
