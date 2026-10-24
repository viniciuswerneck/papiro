#include "LSPManager.h"
#include "LSPClient.h"

#include <QSettings>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace papiro {

// --- LSPServerConfig ---

bool LSPServerConfig::matchesFile(const QString &filePath) const {
    if (fileExtensions.isEmpty())
        return false;

    QFileInfo info(filePath);
    QString ext = info.suffix().toLower();
    if (ext.isEmpty())
        return false;

    QStringList extensions = fileExtensions.split(',', Qt::SkipEmptyParts);
    for (const QString &e : extensions) {
        QString trimmed = e.trimmed().toLower();
        if (trimmed.startsWith('.'))
            trimmed = trimmed.mid(1);
        if (trimmed == ext)
            return true;
    }
    return false;
}

// --- LSPManager ---

LSPManager::LSPManager(QObject *parent)
    : QObject(parent)
{
    loadSettings();
}

LSPManager::~LSPManager() {
    stopAllClients();
    saveSettings();
}

void LSPManager::loadSettings() {
    QSettings settings;
    settings.beginGroup("LSP");

    // Load custom configs
    QStringList keys = settings.childKeys();
    for (const QString &key : keys) {
        if (key.endsWith("/enabled")) {
            QString langId = key.section('/', 0, 0);
            LSPServerConfig config;
            config.languageId = langId;
            config.name = settings.value(langId + "/name", langId).toString();
            config.command = settings.value(langId + "/command", langId).toString();
            config.arguments = settings.value(langId + "/arguments").toStringList();
            config.workingDirectory = settings.value(langId + "/workingDirectory").toString();

            QMap<QString, QString> env;
            QString envStr = settings.value(langId + "/env").toString();
            if (!envStr.isEmpty()) {
                QStringList pairs = envStr.split(';', Qt::SkipEmptyParts);
                for (const QString &pair : pairs) {
                    QStringList kv = pair.split('=', Qt::KeepEmptyParts);
                    if (kv.size() == 2)
                        env[kv[0]] = kv[1];
                }
            }
            config.env = env;

            config.enabled = settings.value(langId + "/enabled", true).toBool();
            config.fileExtensions = settings.value(langId + "/fileExtensions").toString();

            m_configs[langId] = config;
        }
    }

    // If no configs loaded, set up defaults
    if (m_configs.isEmpty()) {
        setupDefaultConfigs();
    }

    m_settingsLoaded = true;
    settings.endGroup();
}

void LSPManager::saveSettings() {
    QSettings settings;
    settings.beginGroup("LSP");

    // Remove old keys
    settings.remove("");

    for (auto it = m_configs.constBegin(); it != m_configs.constEnd(); ++it) {
        const LSPServerConfig &config = it.value();
        QString prefix = config.languageId + "/";

        settings.setValue(prefix + "name", config.name);
        settings.setValue(prefix + "command", config.command);
        settings.setValue(prefix + "arguments", config.arguments);
        settings.setValue(prefix + "workingDirectory", config.workingDirectory);

        QStringList envPairs;
        for (auto envIt = config.env.constBegin(); envIt != config.env.constEnd(); ++envIt) {
            envPairs.append(envIt.key() + "=" + envIt.value());
        }
        settings.setValue(prefix + "env", envPairs.join(';'));

        settings.setValue(prefix + "enabled", config.enabled);
        settings.setValue(prefix + "fileExtensions", config.fileExtensions);
    }

    settings.endGroup();
}

void LSPManager::setupDefaultConfigs() {
    QMap<QString, LSPServerConfig> defaults;

    // C/C++
    defaults["cpp"] = {
        "clangd",
        "cpp",
        "clangd",
        {"--background-index", "--clang-tidy", "--completion-style=detailed", "--header-insertion=never"},
        {},
        {},
        true,
        ".cpp,.h,.hpp,.cc,.cxx,.c,.m,.mm"
    };

    // Python
    defaults["python"] = {
        "pyright",
        "python",
        "pyright-langserver",
        {"--stdio"},
        {},
        {},
        true,
        ".py,.pyw,.pyi"
    };

    // TypeScript/JavaScript
    defaults["typescript"] = {
        "typescript-language-server",
        "typescript",
        "typescript-language-server",
        {"--stdio"},
        {},
        {},
        true,
        ".ts,.tsx,.js,.jsx,.mjs,.cjs"
    };

    // Rust
    defaults["rust"] = {
        "rust-analyzer",
        "rust",
        "rust-analyzer",
        {},
        {},
        {},
        true,
        ".rs"
    };

    // Go
    defaults["go"] = {
        "gopls",
        "go",
        "gopls",
        {},
        {},
        {},
        true,
        ".go"
    };

    // Lua
    defaults["lua"] = {
        "lua-language-server",
        "lua",
        "lua-language-server",
        {},
        {},
        {},
        true,
        ".lua"
    };

    // JSON
    defaults["json"] = {
        "vscode-json-languageserver",
        "json",
        "vscode-json-languageserver",
        {"--stdio"},
        {},
        {},
        true,
        ".json,.jsonc"
    };

    m_configs = std::move(defaults);
    saveSettings(); // Persist defaults
}

void LSPManager::setServerConfig(const LSPServerConfig &config) {
    if (config.languageId.isEmpty()) {
        qWarning() << "LSPManager: Cannot set config with empty languageId";
        return;
    }
    m_configs[config.languageId] = config;
    saveSettings();

    // If client exists, restart it with new config
    auto it = m_clients.find(config.languageId);
    if (it != m_clients.end()) {
        QString langId = config.languageId;
        m_clients.erase(it);
        LSPClient *newClient = getOrCreateClient(langId);
        if (newClient) {
            emit clientStarted(newClient, langId);
        }
    }
}

void LSPManager::removeServerConfig(const QString &languageId) {
    auto it = m_clients.find(languageId);
    if (it != m_clients.end()) {
        it->second->stop();
        m_clients.erase(it);
    }
    m_configs.remove(languageId);
    saveSettings();
}

const LSPServerConfig *LSPManager::configForLanguage(const QString &languageId) const {
    auto it = m_configs.find(languageId);
    if (it != m_configs.end() && it.value().enabled)
        return &it.value();
    return nullptr;
}

const LSPServerConfig *LSPManager::configForFile(const QString &filePath) const {
    for (auto it = m_configs.constBegin(); it != m_configs.constEnd(); ++it) {
        const LSPServerConfig &config = it.value();
        if (config.enabled && config.matchesFile(filePath))
            return &config;
    }
    return nullptr;
}

LSPClient *LSPManager::getOrCreateClient(const QString &languageId) {
    auto it = m_clients.find(languageId);
    if (it != m_clients.end())
        return it->second.get();

    const LSPServerConfig *config = configForLanguage(languageId);
    if (!config) {
        qWarning() << "LSPManager: No enabled config for language" << languageId;
        return nullptr;
    }

    auto client = std::make_unique<LSPClient>(languageId, this);
    LSPClient *clientPtr = client.get();

    // Connect client signals
    connect(clientPtr, &LSPClient::diagnosticsReceived,
            this, &LSPManager::diagnosticsReceived);
    connect(clientPtr, &LSPClient::errorOccurred,
            this, [this, languageId](const QString &error) {
                auto cit = m_clients.find(languageId);
                if (cit != m_clients.end())
                    emit clientError(cit->second.get(), error);
            });
    connect(clientPtr, &LSPClient::stateChanged,
            this, [this, languageId, clientPtr](LSPClient::State state) {
                if (state == LSPClient::State::Ready) {
                    emit clientStarted(clientPtr, languageId);
                } else if (state == LSPClient::State::Stopped || state == LSPClient::State::Error) {
                    emit clientStopped(clientPtr, languageId);
                }
            });

    // Start the client
    bool started = clientPtr->start(config->command, config->arguments,
                                     config->workingDirectory.isEmpty() ? QDir::currentPath() : config->workingDirectory);

    if (!started) {
        qWarning() << "LSPManager: Failed to start client for" << languageId;
        return nullptr;
    }

    m_clients.emplace(languageId, std::move(client));
    return clientPtr;
}

LSPClient *LSPManager::clientForDocument(Document *document) const {
    if (!document)
        return nullptr;

    auto it = m_documentToLanguage.find(document);
    if (it == m_documentToLanguage.end())
        return nullptr;

    const QString &langId = it.value();
    auto clientIt = m_clients.find(langId);
    if (clientIt == m_clients.end())
        return nullptr;

    return clientIt->second.get();
}

void LSPManager::releaseClientForDocument(Document *document) {
    if (document)
        m_documentToLanguage.remove(document);
}

void LSPManager::setWorkspaceFolders(const QVector<LSP::WorkspaceFolder> &folders) {
    m_workspaceFolders = folders;

    // Notify all running clients
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        // LSPClient doesn't have a direct method for this yet,
        // but we could add one if needed. For now, clients get folders at startup.
    }
}

QString LSPManager::languageIdForFile(const QString &filePath) const {
    const LSPServerConfig *config = configForFile(filePath);
    return config ? config->languageId : QString();
}

bool LSPManager::hasServerForLanguage(const QString &languageId) const {
    return configForLanguage(languageId) != nullptr;
}

void LSPManager::stopAllClients() {
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        if (it->second) {
            it->second->stop();
        }
    }
    m_clients.clear();
}

QString LSPManager::fileExtension(const QString &filePath) const {
    QFileInfo info(filePath);
    return info.suffix().toLower();
}

QMap<QString, LSPServerConfig> LSPManager::defaultServerConfigs() {
    QMap<QString, LSPServerConfig> defaults;

    // C/C++
    defaults["cpp"] = {
        "clangd",
        "cpp",
        "clangd",
        {"--background-index", "--clang-tidy", "--completion-style=detailed", "--header-insertion=never"},
        {},
        {},
        true,
        ".cpp,.h,.hpp,.cc,.cxx,.c,.m,.mm"
    };

    // Python
    defaults["python"] = {
        "pyright",
        "python",
        "pyright-langserver",
        {"--stdio"},
        {},
        {},
        true,
        ".py,.pyw,.pyi"
    };

    // TypeScript/JavaScript
    defaults["typescript"] = {
        "typescript-language-server",
        "typescript",
        "typescript-language-server",
        {"--stdio"},
        {},
        {},
        true,
        ".ts,.tsx,.js,.jsx,.mjs,.cjs"
    };

    // Rust
    defaults["rust"] = {
        "rust-analyzer",
        "rust",
        "rust-analyzer",
        {},
        {},
        {},
        true,
        ".rs"
    };

    // Go
    defaults["go"] = {
        "gopls",
        "go",
        "gopls",
        {},
        {},
        {},
        true,
        ".go"
    };

    // Lua
    defaults["lua"] = {
        "lua-language-server",
        "lua",
        "lua-language-server",
        {},
        {},
        {},
        true,
        ".lua"
    };

    // JSON
    defaults["json"] = {
        "vscode-json-languageserver",
        "json",
        "vscode-json-languageserver",
        {"--stdio"},
        {},
        {},
        true,
        ".json,.jsonc"
    };

    return defaults;
}

} // namespace papiro