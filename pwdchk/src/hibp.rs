use crate::{account::Account, error::Error};
use eyre::Result;
use rayon::prelude::*;
use sha1::{Digest, Sha1};
use std::collections::HashMap;

fn sha1(account: &Account) -> (String, String) {
    let mut hasher = Sha1::new();
    hasher.update(account.password.clone());
    let result = hasher.finalize();
    let mut sha1 = format!("{result:X}");
    let sha1_suffix = sha1.split_off(5);
    (sha1, sha1_suffix)
}

fn all_sha1(account: &[Account]) -> Vec<(String, String, &Account)> {
    account
        .par_iter()
        .map(|account| (sha1(account).0, sha1(account).1, account))
        .collect()
}

pub fn sha1_by_prefix(accounts: &[Account]) -> HashMap<String, Vec<(String, &Account)>> {
    let all_sha1 = all_sha1(accounts)
        .into_iter()
        .map(|(prefix, suffix, account)| (prefix, (suffix, account)));
    let mut accounts_by_sha1 = HashMap::<_, Vec<(_, &Account)>>::new();
    for (prefix, account) in all_sha1 {
        accounts_by_sha1
            .entry(prefix)
            .and_modify(|vec| vec.push(account.clone()))
            .or_insert(vec![account]);
    }
    accounts_by_sha1
}

#[allow(dead_code)]
fn get_page(prefix: &str) -> Result<Vec<String>, Error> {
    let mut hibp_url: String = String::from("https://api.pwnedpasswords.com/range/");
    hibp_url.push_str(prefix);
    let body = reqwest::blocking::get(hibp_url)?.text()?;
    Ok(body.lines().map(|str| str.to_string()).collect())
}

#[allow(dead_code)]
fn get_suffixes(prefix: &str) -> Result<HashMap<String, u64>, Error> {
    let hibp_page = get_page(prefix)?;
    let occurences = hibp_page
        .iter()
        .map(|s| s[36..].parse::<u64>())
        .collect::<std::result::Result<Vec<_>, _>>()?;
    let suffixes = hibp_page.into_iter().map(|mut s| {
        s.truncate(35);
        s
    });
    Ok(suffixes.zip(occurences).collect::<HashMap<_, _>>())
}
