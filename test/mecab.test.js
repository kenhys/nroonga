/* eslint no-unused-expressions: 0 */
'use strict'

const fs = require('fs-extra')
const path = require('path')
const expect = require('chai').expect
const nroonga = require('../lib/nroonga')

const testData = [{
  _key: 'http://groonga.org/',
  title: 'groonga - An open-source fulltext search engine and column store'
}, {
  _key: 'http://groonga.rubyforge.org/',
  title: 'Fulltext search by Ruby with groonga - Ranguba'
}, {
  _key: 'http://mroonga.github.com/',
  title: 'Groonga storage engine - Fast fulltext search on MySQL'
}]

/* global beforeEach, afterEach, describe, it */
describe('TokenMecab', () => {
  let db = null
  const tempdir = path.join('test', 'tmp')
  const databaseName = `tempdb-${process.pid}-${(new Date()).valueOf()}`

  beforeEach(() => {
    if (!fs.existsSync(tempdir)) fs.mkdirSync(tempdir)
    db = new nroonga.Database(path.join(tempdir, databaseName))
    db.commandSync('table_create', {
      name: 'Site',
      flags: 'TABLE_HASH_KEY',
      key_type: 'ShortText'
    })
    db.commandSync('column_create', {
      table: 'Site',
      name: 'title',
      flags: 'COLUMN_SCALAR',
      type: 'ShortText'
    })

    db.commandSync('table_create', {
      name: 'Terms',
      flags: 'TABLE_PAT_KEY|KEY_NORMALIZE',
      key_type: 'ShortText',
      default_tokenizer: 'TokenBigram'
    })
    db.commandSync('column_create', {
      table: 'Terms',
      name: 'entry_title',
      flags: 'COLUMN_INDEX|WITH_POSITION',
      type: 'Site',
      source: 'title'
    })

    db.commandSync('load', {
      table: 'Site',
      values: JSON.stringify(testData)
    })
  })

  afterEach(() => {
    db.close()
    fs.removeSync(tempdir)
  })

  describe('#commandSync', () => {
    it('should tokenize tokyo to', () => {
      const matched = db.commandSync('tokenize TokenMecab "東京都"')
      const expected = [{
        value: '東京',
        position: 0,
        force_prefix: false,
        force_prefix_search: false
      },
      {
        value: '都',
        position: 1,
        force_prefix: false,
        force_prefix_search: false
      }]
      expect(matched).to.deep.equal(expected)
    })
  })
})
