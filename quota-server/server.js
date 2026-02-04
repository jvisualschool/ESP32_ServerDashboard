const express = require('express');
const cors = require('cors');
const { exec } = require('child_process');
const os = require('os');

const app = express();
const PORT = 3000;

// CORS 활성화 (ESP32에서 접근 가능하도록)
app.use(cors());
app.use(express.json());

// 로컬 IP 주소 가져오기
function getLocalIP() {
    const interfaces = os.networkInterfaces();
    for (const name of Object.keys(interfaces)) {
        for (const iface of interfaces[name]) {
            if (iface.family === 'IPv4' && !iface.internal) {
                return iface.address;
            }
        }
    }
    return 'localhost';
}

// Antigravity quota 정보 가져오기 (실제 캐시 파일에서)
function getAntigravityQuota(callback) {
    const fs = require('fs');
    const path = require('path');
    const homeDir = os.homedir();
    const quotaDirs = [
        path.join(homeDir, '.antigravity_cockpit', 'cache', 'quota_api_v1_plugin', 'authorized'),
        path.join(homeDir, '.antigravity_cockpit', 'cache', 'quota_api_v1_plugin', 'local'),
        path.join(homeDir, '.antigravity_cockpit', 'cache', 'quota', 'local'),
        path.join(homeDir, '.antigravity_cockpit', 'cache', 'quota', 'authorized')
    ];

    let latestFile = null;
    let latestTime = 0;

    // 모든 폴더 확인하여 가장 최신 파일 찾기
    quotaDirs.forEach(quotaDir => {
        try {
            const files = fs.readdirSync(quotaDir);
            const jsonFiles = files.filter(f => f.endsWith('.json'));
            
            jsonFiles.forEach(file => {
                const filePath = path.join(quotaDir, file);
                const stats = fs.statSync(filePath);
                if (stats.mtimeMs > latestTime) {
                    latestTime = stats.mtimeMs;
                    latestFile = filePath;
                }
            });
        } catch (err) {
            // 폴더가 없거나 읽을 수 없으면 무시
        }
    });

    if (!latestFile) {
        console.log('Quota 파일을 찾을 수 없습니다. 더미 데이터를 사용합니다.');
        callback(getDummyQuotaData());
        return;
    }

    // 가장 최신 파일 읽기
    fs.readFile(latestFile, 'utf8', (err, data) => {
        if (err) {
            console.error('Quota 파일 읽기 오류:', err);
            callback(getDummyQuotaData());
            return;
        }

        try {
            const quotaData = JSON.parse(data);
            console.log(`✅ Quota 파일 로드: ${path.basename(latestFile)} (${quotaData.source || 'unknown'})`);
            callback(parseQuotaData(quotaData));
        } catch (e) {
            console.error('JSON 파싱 오류:', e);
            callback(getDummyQuotaData());
        }
    });
}

// Antigravity quota 데이터를 ESP32 형식으로 변환
function parseQuotaData(quotaData) {
    const now = new Date();
    
    // 새로운 형식 (quota_api_v1_plugin)
    if (quotaData.payload && quotaData.payload.models) {
        const models = quotaData.payload.models;
        const quotas = [];
        
        for (const [modelId, modelData] of Object.entries(models)) {
            if (modelData.recommended && modelData.displayName && !modelData.displayName.includes('Gemini 2.5')) {
                const quotaInfo = modelData.quotaInfo;
                if (!quotaInfo) continue;
                
                const remainingFraction = (quotaInfo.remainingFraction !== undefined && quotaInfo.remainingFraction !== null) ? quotaInfo.remainingFraction : 0;
                const remainingPercentage = Math.round(remainingFraction * 100);
                const resetTime = new Date(quotaInfo.resetTime);
                const diffMs = resetTime - now;
                const diffHours = Math.floor(diffMs / (1000 * 60 * 60));
                const diffMinutes = Math.floor((diffMs % (1000 * 60 * 60)) / (1000 * 60));
                
                let resetTimeStr;
                if (diffHours > 0) {
                    resetTimeStr = `${diffHours}h ${diffMinutes}m`;
                } else if (diffMinutes > 0) {
                    resetTimeStr = `${diffMinutes}m`;
                } else {
                    resetTimeStr = 'Soon';
                }

                let status = 'green';
                if (remainingPercentage <= 20) {
                    status = 'red';
                } else if (remainingPercentage <= 50) {
                    status = 'yellow';
                }

                quotas.push({
                    name: modelData.displayName,
                    percent: remainingPercentage,
                    reset_time: resetTimeStr,
                    status: status,
                    date: resetTime.toLocaleDateString('en-US', { day: '2-digit', month: '2-digit' }).replace(/\//g, '.')
                });
            }
        }
        
        return {
            updated_at: new Date(quotaData.updatedAt).toISOString(),
            quotas: quotas
        };
    }
    
    // 기존 형식 (quota)
    const quotas = quotaData.models
        .filter(model => model.isRecommended) // 추천 모델만
        .filter(model => !model.displayName.includes('Gemini 2.5')) // Gemini 2.5 제외
        .map(model => {
            const resetTime = new Date(model.resetTime);
            const diffMs = resetTime - now;
            const diffHours = Math.floor(diffMs / (1000 * 60 * 60));
            const diffMinutes = Math.floor((diffMs % (1000 * 60 * 60)) / (1000 * 60));
            
            let resetTimeStr;
            if (diffHours > 0) {
                resetTimeStr = `${diffHours}h ${diffMinutes}m`;
            } else if (diffMinutes > 0) {
                resetTimeStr = `${diffMinutes}m`;
            } else {
                resetTimeStr = 'Soon';
            }

            const percent = (model.remainingPercentage !== undefined && model.remainingPercentage !== null) ? model.remainingPercentage : 0;
            let status = 'green';
            if (percent <= 20) {
                status = 'red';
            } else if (percent <= 50) {
                status = 'yellow';
            }

            return {
                name: model.displayName,
                percent: percent,
                reset_time: resetTimeStr,
                status: status,
                date: resetTime.toLocaleDateString('en-US', { day: '2-digit', month: '2-digit' }).replace(/\//g, '.')
            };
        });

    return {
        updated_at: new Date(quotaData.updatedAt).toISOString(),
        quotas: quotas
    };
}

// 더미 quota 데이터 (테스트용)
function getDummyQuotaData() {
    const now = new Date();
    const resetTime = new Date(now);
    resetTime.setHours(resetTime.getHours() + 4);
    resetTime.setMinutes(resetTime.getMinutes() + 46);

    return {
        updated_at: now.toISOString(),
        quotas: [
            {
                name: "Claude Opus 4.5 (Thinking)",
                percent: 0,
                reset_time: "4h 46m",
                status: "red",
                date: "02."
            },
            {
                name: "Claude Sonnet 4.5",
                percent: 0,
                reset_time: "4h 46m",
                status: "red",
                date: "02."
            }
        ]
    };
}

// API 엔드포인트
app.get('/quota.json', (req, res) => {
    getAntigravityQuota((data) => {
        if (data) {
            res.json(data);
        } else {
            res.status(500).json({ error: 'Quota 정보를 가져올 수 없습니다.' });
        }
    });
});

// 서버 시작
app.listen(PORT, '0.0.0.0', () => {
    const localIP = getLocalIP();
    console.log('\n🚀 Antigravity Quota API Server');
    console.log('================================');
    console.log(`✅ 서버 실행 중: http://localhost:${PORT}`);
    console.log(`✅ 로컬 네트워크: http://${localIP}:${PORT}`);
    console.log(`\n📡 ESP32에서 사용할 URL: http://${localIP}:${PORT}/quota.json`);
    console.log('================================\n');
});
