#include <QSettings>
#include <QFile>
#include <QNetworkCookie>
#include <iostream>
#include "page.h"
#include "json.h"
#include "math.h"



Page::Page(Site *site, QMap<QString,Site*> *sites, QStringList tags, int page, int limit, QStringList postFiltering, bool smart, QObject *parent, int pool) : QObject(parent), m_site(site), m_postFiltering(postFiltering), m_errors(QStringList()), m_imagesPerPage(limit), m_currentSource(0), m_smart(smart)
{
	m_website = m_site->url();
	m_imagesCount = -1;
	m_pagesCount = -1;

    // Replace shortcuts to increase compatibility
	QString text = " "+tags.join(" ")+" ";
	text.replace(" rating:s ", " rating:safe ", Qt::CaseInsensitive)
		.replace(" rating:q ", " rating:questionable ", Qt::CaseInsensitive)
		.replace(" rating:e ", " rating:explicit ", Qt::CaseInsensitive)
		.replace(" -rating:s ", " -rating:safe ", Qt::CaseInsensitive)
		.replace(" -rating:q ", " -rating:questionable ", Qt::CaseInsensitive)
        .replace(" -rating:e ", " -rating:explicit ", Qt::CaseInsensitive);
	tags = text.split(" ", QString::SkipEmptyParts);
	tags.removeDuplicates();

    // Get the list of all enabled modifiers
	QStringList modifiers = QStringList();
	for (int i = 0; i < sites->size(); i++)
	{
		if (sites->value(sites->keys().at(i))->contains("Modifiers"))
		{ modifiers.append(sites->value(sites->keys().at(i))->value("Modifiers").trimmed().split(" ", QString::SkipEmptyParts)); }
	}
	if (m_site->contains("Modifiers"))
	{
		QStringList mods = m_site->value("Modifiers").trimmed().split(" ", QString::SkipEmptyParts);
		for (int j = 0; j < mods.size(); j++)
		{ modifiers.removeAll(mods[j]); }
	}
	for (int k = 0; k < modifiers.size(); k++)
	{ tags.removeAll(modifiers[k]); }
	m_search = tags;

    // Set values
	m_page = page;
	m_pool = pool;
	fallback(false);
	m_replyExists = false;
	m_replyTagsExists = false;
	m_currentUrl = 0;
}
Page::~Page()
{
	// qDeleteAll(m_images);
}

void Page::fallback(bool bload)
{
	if (m_currentSource > m_site->value("Selected").count('/'))
	{
		log(tr("Aucune source valide du site n'a retourné de résultat."));
		m_errors.append(tr("Aucune source valide du site n'a retourné de résultat."));
		m_currentSource++;
		//emit finishedLoading(this);
		return;
	}
	if (m_currentSource > 0)
	{ log(tr("Chargement en %1 échoué. Nouvel essai en %2.").arg(m_format).arg(m_site->value("Selected").split('/').at(m_currentSource))); }

	m_currentSource++;

	// Default tag is none is given
	QString t = m_search.join(" ").trimmed();
	if (m_site->contains("DefaultTag") && t.isEmpty())
	{ t = m_site->value("DefaultTag"); }

	// Find page number
	m_format = m_site->value("Selected").split('/').at(m_currentSource-1);
	int p = m_page;
	m_blim = m_site->contains("Urls/"+QString::number(m_currentSource)+"/Limit") ? m_site->value("Urls/"+QString::number(m_currentSource)+"/Limit").toInt() : m_imagesPerPage;
	if (m_smart)
	{
		if (m_imagesPerPage > m_blim)
		{ m_imagesPerPage = m_blim; }
		p = (int)floor(((m_page - 1.) * m_imagesPerPage) / m_blim) + 1;
	}
	p = p - 1 + m_site->value("FirstPage").toInt();

	// Check if we are looking for a pool
	QRegExp pool("pool:(\\d+)");
	QString url;
	int pl = -1;
	int pos = -1;
	if ((pos = pool.indexIn(t)) != -1)
	{
		for (int i = 1; i <= m_site->value("Selected").count('/') + 1; i++)
		{
			if (m_site->contains("Urls/"+QString::number(i)+"/Pools"))
			{
				url = m_site->value("Urls/"+QString::number(i)+"/Pools");
				url.replace("{pool}", pool.cap(1));
				pl = pool.cap(1).toInt();
				m_currentSource = i;
				m_format = m_site->value("Selected").split('/').at(m_currentSource-1);
				t = t.remove(pos, pool.cap(0).length()).trimmed();
				break;
			}
		}
		if (url.isEmpty())
		{
			log(tr("Aucune source du site n'est compatible avec les pools."));
			m_errors.append(tr("Aucune source du site n'est compatible avec les pools."));
			m_search.removeAll("pool:"+pool.cap(1));
			t.remove(pool);
			t = t.trimmed();
		}
	}
	if (url.isEmpty())
	{
		if (t.isEmpty() && m_site->contains("Urls/"+QString::number(m_currentSource)+"/Home"))
		{ url = m_site->value("Urls/"+QString::number(m_currentSource)+"/Home"); }
		else
		{ url = m_site->value("Urls/"+QString::number(m_currentSource)+"/Tags"); }
	}

	// GET login information
	QSettings settings(savePath("settings.ini"), QSettings::IniFormat);
	QString pseudo = m_site->setting("auth/pseudo", settings.value("Login/pseudo", "").toString()).toString();
	QString password = m_site->setting("auth/password", settings.value("Login/password", "").toString()).toString();

	// Global replace tokens
	m_originalUrl = QString(url);
	url.replace("{page}", QString::number(p));
	url.replace("{tags}", QUrl::toPercentEncoding(t));
	url.replace("{limit}", QString::number(m_imagesPerPage));
	url.replace("{pseudo}", pseudo);
	url.replace("{password}", password);
	m_url = m_site->fixUrl(url);

	if ((pl >= 0 || pool.indexIn(t) != -1) && m_site->contains("Urls/Html/Pools"))
	{
		QString url = m_site->value("Urls/Html/Pools");
		url.replace("{page}", QString::number(p));
		url.replace("{pool}", pool.cap(1));
		url.replace("{tags}", QUrl::toPercentEncoding(t));
		url.replace("{limit}", QString::number(m_imagesPerPage));
		url.replace("{pseudo}", pseudo);
		url.replace("{password}", password);
		m_urlRegex = m_site->fixUrl(url);
	}
	else if (m_site->contains("Urls/Html/Tags"))
	{
		QString url = m_site->value("Urls/Html/"+QString(t.isEmpty() && m_site->contains("Urls/Html/Home") ? "Home" : "Tags"));
		url.replace("{page}", QString::number(p));
		url.replace("{tags}", QUrl::toPercentEncoding(t));
		url.replace("{limit}", QString::number(m_imagesPerPage));
		url.replace("{pseudo}", pseudo);
		url.replace("{password}", password);
		m_urlRegex = m_site->fixUrl(url);
	}
	else
	{ m_urlRegex = ""; }

	if (bload)
	{ load(); }
}

void Page::load()
{
	if (m_currentSource <= m_site->value("Selected").count('/') + 1)
	{
		m_reply = m_site->get(m_url);
		connect(m_reply, SIGNAL(finished()), this, SLOT(parse()));
		m_replyExists = true;
	}
}
void Page::abort()
{
	if (m_replyExists)
	{
		if (m_reply->isRunning())
		{ m_reply->abort(); }
	}
}

void Page::loadTags()
{
	if (!m_urlRegex.isEmpty())
	{
		m_replyTags = m_site->get(m_urlRegex);
		connect(m_replyTags, SIGNAL(finished()), this, SLOT(parseTags()));
		m_replyTagsExists = true;
	}
}
void Page::abortTags()
{
	if (m_replyTagsExists)
	{
		if (m_replyTags->isRunning())
		{ m_replyTags->abort(); }
	}
}

QString _parseSetImageUrl(Site* site, QString setting, QString ret, QMap<QString,QString> *d, bool replaces = true)
{
	if (site->contains(setting) && replaces)
    {
        if (site->value(setting).contains("->"))
        {
            QStringList replaces = site->value(setting).split('&');
			for (QString rep : replaces)
            {
                QRegExp rgx(rep.left(rep.indexOf("->")));
                ret.replace(rgx, rep.right(rep.size() - rep.indexOf("->") - 2));
			}
        }
        else
        {
            ret = site->value(setting);
            ret.replace("{id}", d->value("id"))
            .replace("{md5}", d->value("md5"))
            .replace("{ext}", d->value("ext"));
        }
    }
	return site->fixUrl(ret).toString();
}


void Page::parseImage(QMap<QString,QString> d, int position)
{
	// Set default values
	if (!d.contains("ext") || d["ext"].isEmpty())
	{ d["ext"] = "jpg"; }
	if (!d.contains("file_url"))
	{ d["file_url"] = d["preview_url"]; }
	if (!d.contains("sample_url"))
	{ d["sample_url"] = d["preview_url"]; }

	// Fix urls
	d["file_url"] = _parseSetImageUrl(m_site, "Urls/"+QString::number(m_currentSource)+"/Image", d["file_url"], &d);
	d["sample_url"] = _parseSetImageUrl(m_site, "Urls/"+QString::number(m_currentSource)+"/Sample", d["sample_url"], &d);
	d["preview_url"] = _parseSetImageUrl(m_site, "Urls/"+QString::number(m_currentSource)+"/Preview", d["preview_url"], &d);

	// Page URL
	if (!d.contains("page_url"))
	{
		d["page_url"] = m_site->value("Urls/Html/Post");
		QString t = m_search.join(" ");
		if (m_site->contains("DefaultTag") && t.isEmpty())
		{ t = m_site->value("DefaultTag"); }
		d["page_url"].replace("{tags}", QUrl::toPercentEncoding(t));
		d["page_url"].replace("{id}", d["id"]);
	}

	// Generate image
	Image *img = new Image(d, this);
	QString error = img->filter(m_postFiltering);

	// If the file path is wrong (ends with "/.jpg")
	if (error.isEmpty() && d["file_url"].endsWith("/." + d["ext"]))
	{ error = "file url"; }

	// Add if everything is ok
	if (error.isEmpty())
	{ m_images.append(img); }
	else
	{
		img->deleteLater();
		log(tr("Image #%1 ignorée. Raison : %2.").arg(QString::number(position + 1), error));
	}
}

void Page::parse()
{
    m_source = m_reply->readAll();

	// Check redirection
	QUrl redir = m_reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
	if (!redir.isEmpty())
	{
		m_url = m_site->fixUrl(redir.toString(), m_url);
		load();
		return;
	}

	// Reading reply and resetting vars
	qDeleteAll(m_images);
	m_images.clear();
	m_tags.clear();
	/*m_imagesCount = -1;
	m_pagesCount = -1;*/

	if (m_source.isEmpty())
	{
		if (m_reply->error() != QNetworkReply::OperationCanceledError)
		{
			log("Loading error: "+m_reply->errorString());
			fallback();
		}
		return;
	}

	int first = m_smart ? ((m_page - 1) * m_imagesPerPage) % m_blim : 0;

	// XML
	if (m_format == "xml")
	{
		// Initializations
		QDomDocument doc;
		QString errorMsg;
		int errorLine, errorColumn;
		if (!doc.setContent(m_source, false, &errorMsg, &errorLine, &errorColumn))
		{
			log(tr("Erreur lors de l'analyse du fichier XML : %1 (%2 - %3).").arg(errorMsg, QString::number(errorLine), QString::number(errorColumn)));
			fallback();
			return;
		}
		QDomElement docElem = doc.documentElement();

		// Getting last page
		int count = docElem.attributes().namedItem("count").nodeValue().toFloat();
		QString database = docElem.attributes().namedItem("type").nodeValue();
		if (count == 0 && database == "array")
		{ count = docElem.elementsByTagName("total-count").at(0).toElement().text().toInt(); }
		if (count > 0)
		{ m_imagesCount = count; }

		// Reading posts
		QDomNodeList nodeList = docElem.elementsByTagName("post");
		if (nodeList.count() > 0)
		{
			for (int id = 0; id < nodeList.count(); id++)
			{
				QMap<QString,QString> d;
				if (database == "array")
				{
					QStringList infos, assoc;
					infos << "created_at" << "status" << "source" << "has_comments" << "file_url" << "sample_url" << "change" << "sample_width" << "has_children" << "preview_url" << "width" << "md5" << "preview_width" << "sample_height" << "parent_id" << "height" << "has_notes" << "creator_id" << "file_size" << "id" << "preview_height" << "rating" << "tags" << "author" << "score" << "tags_artist" << "tags_character" << "tags_copyright" << "tags_general" << "ext";
					assoc << "created-at" << "status" << "source" << "has_comments" << "file-url" << "large-file-url" << "change" << "sample_width" << "has-children" << "preview-file-url" << "image-width" << "md5" << "preview_width" << "sample_height" << "parent-id" << "image-height" << "has_notes" << "uploader-id" << "file_size" << "id" << "preview_height" << "rating" << "tag-string" << "uploader-name" << "score" << "tag-string-artist" << "tag-string-character" << "tag-string-copyright" << "tag-string-general" << "file-ext";
					for (int i = 0; i < infos.count(); i++)
					{ d[infos.at(i)] = nodeList.at(id + first).namedItem(assoc.at(i)).toElement().text(); }
				}
				else
				{
					QStringList infos;
					infos << "created_at" << "status" << "source" << "has_comments" << "file_url" << "sample_url" << "change" << "sample_width" << "has_children" << "preview_url" << "width" << "md5" << "preview_width" << "sample_height" << "parent_id" << "height" << "has_notes" << "creator_id" << "file_size" << "id" << "preview_height" << "rating" << "tags" << "author" << "score";
					for (int i = 0; i < infos.count(); i++)
					{ d[infos.at(i)] = nodeList.at(id + first).attributes().namedItem(infos.at(i)).nodeValue().trimmed(); }
				}
				this->parseImage(d, id + first);
			}
		}
	}

	// RSS
	else if (m_format == "rss")
	{
		// Initializations
		QDomDocument doc;
		QString errorMsg;
		int errorLine, errorColumn;
		if (!doc.setContent(m_source, false, &errorMsg, &errorLine, &errorColumn))
		{
			log(tr("Erreur lors de l'analyse du fichier RSS : %1 (%2 - %3).").arg(errorMsg, QString::number(errorLine), QString::number(errorColumn)));
			fallback();
			return;
		}
		QDomElement docElem = doc.documentElement();

		// Reading posts
		QDomNodeList nodeList = docElem.elementsByTagName("item");
		if (nodeList.count() > 0)
		{
			for (int id = 0; id < nodeList.count(); id++)
			{
				QDomNodeList children = nodeList.at(id + first).childNodes();
				QMap<QString,QString> d, dat;
				for (int i = 0; i < children.size(); i++)
				{
					QString content = children.at(i).childNodes().at(0).nodeValue();
					if (!content.isEmpty())
					{ dat.insert(children.at(i).nodeName(), content.trimmed()); }
					else
					{ dat.insert(children.at(i).nodeName(), children.at(i).attributes().namedItem("url").nodeValue().trimmed()); }
				}
				// QDateTime::fromString(date, "ddd, dd MMM yyyy hh:mm:ss +0000"); // shimmie date format
				d.insert("page_url", dat["link"]);
				d.insert("tags", dat["media:keywords"]);
				d.insert("preview_url", dat["media:thumbnail"]);
				d.insert("sample_url", dat["media:content"]);
				d.insert("file_url", dat["media:content"]);
				if (!d.contains("id"))
				{
					QRegExp rx("/(\\d+)");
					rx.indexIn(d["page_url"]);
					d.insert("id", rx.cap(1));
				}
				this->parseImage(d, id + first);
			}
		}
	}

	// Regexes
	else if (m_format == "regex")
	{
		// Getting tags
		if (m_site->contains("Regex/Tags"))
		{
			QRegExp rxtags(m_site->value("Regex/Tags"));
			rxtags.setMinimal(true);
			QStringList tags = QStringList();
			int p = 0;
			while (((p = rxtags.indexIn(m_source, p)) != -1))
			{
				if (!tags.contains(rxtags.cap(2)))
				{
					m_tags.append(Tag(rxtags.cap(2), rxtags.cap(1), rxtags.cap(3).toInt()));
					tags.append(rxtags.cap(2));
				}
				p += rxtags.matchedLength();
			}
		}

		// Getting images
		QRegExp rx(m_site->value("Regex/Image"));
		QStringList order = m_site->value("Regex/Order").split('|');
		rx.setMinimal(true);
		int pos = 0, id = 0;
		while ((pos = rx.indexIn(m_source, pos)) != -1)
        {
			pos += rx.matchedLength();
			QMap<QString,QString> d;
			for (int i = 0; i < order.size(); i++)
			{ d[order.at(i)] = rx.cap(i+1); }

			// JSON elements
			if (order.contains("json") && !d["json"].isEmpty())
			{
				QVariant src = Json::parse(d["json"]);
				if (!src.isNull())
				{
					QMap<QString,QVariant> map = src.toMap();
					for (int i = 0; i < map.size(); i++)
					{ d[map.keys().at(i)] = map.values().at(i).toString(); }
				}
			}
			this->parseImage(d, id + first);
			id++;
		}
	}

	// JSON
	else if (m_format == "json")
	{
		QVariant src = Json::parse(m_source);
		if (!src.isNull())
		{
			QMap<QString, QVariant> sc;
			QList<QVariant> sourc = src.toList();
			if (sourc.isEmpty())
			{ sourc = src.toMap().value("images").toList(); }
			for (int id = 0; id < sourc.count(); id++)
			{
				sc = sourc.at(id + first).toMap();
				QMap<QString,QString> d;
				if (sc.contains("tag_string"))
				{
					QStringList infos, assoc;
					infos << "created_at" << "status" << "source" << "has_comments" << "file_url" << "sample_url" << "change" << "sample_width" << "has_children" << "preview_url" << "width" << "md5" << "preview_width" << "sample_height" << "parent_id" << "height" << "has_notes" << "creator_id" << "file_size" << "id" << "preview_height" << "rating" << "tags" << "author" << "score" << "tags_artist" << "tags_character" << "tags_copyright" << "tags_general";
					assoc << "created_at" << "status" << "source" << "has_comments" << "file_url" << "large_file_url" << "change" << "sample_width" << "has_children" << "preview_file_url" << "image_width" << "md5" << "preview_width" << "sample_height" << "parent_id" << "image_height" << "has_notes" << "uploader_id" << "file_size" << "id" << "preview_height" << "rating" << "tag_string" << "uploader_name" << "score" << "tag_string_artist" << "tag_string_character" << "tag_string_copyright" << "tag_string_general";
					for (int i = 0; i < infos.count(); i++)
					{ d[infos.at(i)] = sc.value(assoc.at(i)).toString().trimmed(); }
				}
				else if (sc.contains("tag_ids"))
				{
					QStringList infos, assoc;
					infos << "created_at" << "source" << "file_url" << "preview_url" << "width" << "md5" << "height" << "id" << "tags" << "author" << "score";
					assoc << "created_at" << "source_url" << "image" << "image" << "width" << "id" << "height" << "id_number" << "tags" << "uploader" << "score";
					for (int i = 0; i < infos.count(); i++)
					{ d[infos.at(i)] = sc.value(assoc.at(i)).toString().trimmed(); }
				}
				else
				{
					QStringList infos;
					infos << "created_at" << "status" << "source" << "has_comments" << "file_url" << "sample_url" << "change" << "sample_width" << "has_children" << "preview_url" << "width" << "md5" << "preview_width" << "sample_height" << "parent_id" << "height" << "has_notes" << "creator_id" << "file_size" << "id" << "preview_height" << "rating" << "tags" << "author" << "score";
					for (int i = 0; i < infos.count(); i++)
					{ d[infos.at(i)] = sc.value(infos.at(i)).toString().trimmed(); }
                }
				this->parseImage(d, id + first);
			}
		}
		else
		{
			fallback();
			return;
		}
	}

	// If tags have not been retrieved yet
	if (m_tags.isEmpty())
	{
		QStringList tagsGot;
		for (int i = 0; i < m_images.count(); i++)
		{
			QList<Tag> tags = m_images.at(i)->tags();
			for (int t = 0; t < tags.count(); t++)
			{
				if (tagsGot.contains(tags[t].text()))
				{ m_tags[tagsGot.indexOf(tags[t].text())].setCount(m_tags[tagsGot.indexOf(tags[t].text())].count()+1); }
				else
				{
					m_tags.append(tags[t]);
					tagsGot.append(tags[t].text());
				}
			}
		}
	}

	// Getting last page
	if (m_site->contains("LastPage") && m_pagesCount < 1)
	{ m_pagesCount = m_site->value("LastPage").toInt(); }
	if (m_site->contains("Regex/Count") && m_imagesCount < 1)
	{
		QRegExp rxlast(m_site->value("Regex/Count"));
		rxlast.indexIn(m_source, 0);
		m_imagesCount = rxlast.cap(1).remove(",").toInt();
	}
	if (m_site->contains("Regex/LastPage") && m_pagesCount < 1)
	{
		QRegExp rxlast(m_site->value("Regex/LastPage"));
		rxlast.indexIn(m_source, 0);
		m_pagesCount = rxlast.cap(1).remove(",").toInt();
	}

	// Guess images count
	if (m_site->contains("Urls/"+QString::number(m_currentSource)+"/Limit") && m_pagesCount > 0)
	{ m_imagesCount = m_pagesCount * m_site->value("Urls/"+QString::number(m_currentSource)+"/Limit").toInt(); }

	// Remove first n images (according to site settings)
	int skip = m_site->setting("ignore/always", 0).toInt();
	if (m_page == m_site->value("FirstPage").toInt())
		skip = m_site->setting("ignore/1", 0).toInt();
	if (m_images.size() > m_imagesPerPage && m_images.size() > skip)
		for (int i = 0; i < skip; ++i)
			m_images.removeFirst();

	// Virtual paging
	int firstImage = 0;
	int lastImage = m_smart ? m_imagesPerPage : m_images.size();
	if (!m_originalUrl.contains("{page}"))
	{
		firstImage = m_imagesPerPage * (m_page - 1);
		lastImage = m_imagesPerPage;
	}
    while (firstImage > 0)
	{
		m_images.removeFirst();
		firstImage--;
	}
	while (m_images.size() > lastImage)
	{ m_images.removeLast(); }

	m_reply->deleteLater();
	m_replyExists = false;

	QString t = m_search.join(" ");
	if (m_site->contains("DefaultTag") && t.isEmpty())
	{ t = m_site->value("DefaultTag"); }
	if (!m_search.isEmpty() && !m_site->value("Urls/"+QString::number(m_currentSource)+"/"+(t.isEmpty() && !m_site->contains("Urls/"+QString::number(m_currentSource)+"/Home") ? "Home" : "Tags")).contains("{tags}"))
	{ m_errors.append(tr("La recherche par tags est impossible avec la source choisie (%1).").arg(m_format)); }

	emit finishedLoading(this);
}
void Page::parseTags()
{
	// Check redirection
	QUrl redir = m_replyTags->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
	if (!redir.isEmpty())
	{
		m_urlRegex = m_site->fixUrl(redir.toString(), m_urlRegex);
		loadTags();
		return;
	}

	QString source = QString::fromUtf8(m_replyTags->readAll());
	m_tags.clear();

	if (m_site->contains("Regex/Tags"))
	{
		QRegExp rxtags(m_site->value("Regex/Tags"));
		rxtags.setMinimal(true);
		int p = 0;
		QStringList order = m_site->value("Regex/TagsOrder").split('|', QString::SkipEmptyParts);
		while ((p = rxtags.indexIn(source, p)) != -1)
		{
			p += rxtags.matchedLength();
			QString type = "", tag = "";
			int count = 1;
			if (order.empty())
			{
				switch (rxtags.captureCount())
				{
					case 4:	order << "type" << "" << "count" << "tag";	break;
					case 3:	order << "type" << "tag" << "count";		break;
					case 2:	order << "type" << "tag";					break;
					case 1:	order << "tag";								break;
				}
			}
			for (int o = 0; o < order.size(); o++)
			{
				if (order.at(o) == "tag" && tag.isEmpty())
				{ tag = rxtags.cap(o + 1).replace(" ", "_").replace("&amp;", "&").trimmed(); }
				else if (order.at(o) == "type" && type.isEmpty())
				{
					type = rxtags.cap(o + 1).toLower().trimmed();
					if (type.contains(", "))
					{ type = type.split(", ").at(0).trimmed(); }
					if (type == "series")
					{ type = "copyright"; }
					else if (type == "mangaka")
					{ type = "artist"; }
					else if (type == "game")
					{ type = "copyright"; }
					else if (type == "studio")
					{ type = "circle"; }
					else if (type == "source")
					{ type = "general"; }
					else if (type == "character group")
					{ type = "general"; }
					else if (type.length() == 1)
					{
						int tpe = type.toInt();
						if (tpe >= 0 && tpe <= 4)
						{
							QStringList types = QStringList() << "general" << "artist" << "unknown" << "copyright" << "character";
							type = types[tpe];
						}
					}
				}
				else if (order.at(o) == "count" && count != 0)
				{ count = rxtags.cap(o + 1).toLower().endsWith('k') ? rxtags.cap(3).left(rxtags.cap(3).length() - 1).toInt() * 1000 : rxtags.cap(3).toInt(); }
			}
			if (type.isEmpty())
			{ type = "unknown"; }
			m_tags.append(Tag(tag, type, count));
		}
	}

	// Getting last page
	if (m_site->contains("Regex/Count") && m_imagesCount < 1)
	{
		QRegExp rxlast(m_site->value("Regex/Count"));
		rxlast.indexIn(source, 0);
		m_imagesCount = rxlast.cap(1).remove(",").toInt();
	}
	if (m_imagesCount < 1)
	{
		for (Tag tag : m_tags)
		{
			if (tag.text() == m_search.join(" "))
			{
				m_imagesCount = tag.count();
				if (m_pagesCount < 0)
					m_pagesCount = (int)ceil((m_imagesCount * 1.) / m_imagesPerPage);
			}
		}
	}
	if (m_site->contains("Regex/LastPage") && (m_imagesCount < 1 || m_imagesCount % 1000 == 0))
	{
		QRegExp rxlast(m_site->value("Regex/LastPage"));
		rxlast.indexIn(source, 0);
		m_pagesCount = rxlast.cap(1).remove(",").toInt();
		if (m_pagesCount != 0)
			m_imagesCount = m_pagesCount * m_imagesPerPage;
	}

	// Wiki
	m_wiki.clear();
	if (m_site->contains("Regex/Wiki"))
	{
		QRegExp rxwiki(m_site->value("Regex/Wiki"));
		rxwiki.setMinimal(true);
		if (rxwiki.indexIn(source) != -1)
		{
			m_wiki = rxwiki.cap(1);
			m_wiki.remove("/wiki/show?title=").remove(QRegExp("<p><a href=\"([^\"]+)\">Full entry &raquo;</a></p>")).replace("<h6>", "<span class=\"title\">").replace("</h6>", "</span>");
		}
	}

	m_replyTags->deleteLater();
	m_replyTagsExists = false;

	emit finishedLoadingTags(this);
}
void Page::clear()
{
	m_images.clear();
}

QList<Image*>	Page::images()		{ return m_images;		}
Site			*Page::site()		{ return m_site;		}
QUrl			Page::url()			{ return m_url;			}
QString			Page::source()		{ return m_source;		}
QString			Page::website()		{ return m_website;		}
QString			Page::wiki()		{ return m_wiki;		}
QList<Tag>		Page::tags()		{ return m_tags;		}
QStringList		Page::search()		{ return m_search;		}
QStringList		Page::errors()		{ return m_errors;		}

int Page::imagesCount(bool guess)
{
	if (m_imagesCount < 0 && guess && m_pagesCount >= 0)
		return m_pagesCount * m_imagesPerPage;
	return m_imagesCount;
}
int Page::pagesCount(bool guess)
{
	if (m_pagesCount < 0 && guess && m_imagesCount >= 0)
		return (int)ceil((m_imagesCount * 1.) / m_imagesPerPage);
	return m_pagesCount;
}
