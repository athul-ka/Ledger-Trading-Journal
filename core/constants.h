#pragma once

#include <QString>
#include <QStringList>

namespace Domain {

inline QStringList setupOptions()
{
    return {"Simple", "2-Step", "Perfect", "Perfect-Counter"};
}

inline QStringList accountOptions()
{
    return {"Funded 1", "Funded 2", "Live"};
}

inline double startingBalanceForAccount(const QString &account)
{
    if (account == "Funded 1" || account == "Funded 2") {
        return 5000.0;
    }
    if (account == "Live") {
        return 1000.0;
    }
    return 0.0;
}

inline QString normalizeAccountName(const QString &rawAccount)
{
    const QString compact = rawAccount.simplified();
    if (compact.compare("Funded 1", Qt::CaseInsensitive) == 0) {
        return "Funded 1";
    }
    if (compact.compare("Funded 2", Qt::CaseInsensitive) == 0) {
        return "Funded 2";
    }
    if (compact.compare("Live", Qt::CaseInsensitive) == 0) {
        return "Live";
    }
    return compact;
}

inline QStringList parseAccounts(const QString &accountsText)
{
    QStringList result;
    const QStringList parts = accountsText.split(',', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString normalized = normalizeAccountName(part);
        if (!normalized.isEmpty()) {
            result << normalized;
        }
    }
    result.removeDuplicates();
    return result;
}

inline QString joinAccounts(const QStringList &accounts)
{
    QStringList normalizedAccounts;
    for (const QString &account : accounts) {
        const QString normalized = normalizeAccountName(account);
        if (!normalized.isEmpty()) {
            normalizedAccounts << normalized;
        }
    }
    normalizedAccounts.removeDuplicates();
    return normalizedAccounts.join(", ");
}

} // namespace Domain
